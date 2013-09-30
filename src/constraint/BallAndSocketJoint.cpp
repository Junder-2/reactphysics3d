/********************************************************************************
* ReactPhysics3D physics library, http://code.google.com/p/reactphysics3d/      *
* Copyright (c) 2010-2013 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

// Libraries
#include "BallAndSocketJoint.h"
#include "../engine/ConstraintSolver.h"

using namespace reactphysics3d;

// Static variables definition
const decimal BallAndSocketJoint::BETA = decimal(0.2);

// Constructor
BallAndSocketJoint::BallAndSocketJoint(const BallAndSocketJointInfo& jointInfo)
                   : Joint(jointInfo), mImpulse(Vector3(0, 0, 0)) {

    // Compute the local-space anchor point for each body
    mLocalAnchorPointBody1 = mBody1->getTransform().getInverse() * jointInfo.anchorPointWorldSpace;
    mLocalAnchorPointBody2 = mBody2->getTransform().getInverse() * jointInfo.anchorPointWorldSpace;
}

// Destructor
BallAndSocketJoint::~BallAndSocketJoint() {

}

// Initialize before solving the constraint
void BallAndSocketJoint::initBeforeSolve(const ConstraintSolverData& constraintSolverData) {

    // Initialize the bodies index in the velocity array
    mIndexBody1 = constraintSolverData.mapBodyToConstrainedVelocityIndex.find(mBody1)->second;
    mIndexBody2 = constraintSolverData.mapBodyToConstrainedVelocityIndex.find(mBody2)->second;

    // Get the bodies positions and orientations
    const Vector3& x1 = mBody1->getTransform().getPosition();
    const Vector3& x2 = mBody2->getTransform().getPosition();
    const Quaternion& orientationBody1 = mBody1->getTransform().getOrientation();
    const Quaternion& orientationBody2 = mBody2->getTransform().getOrientation();

    // Get the inertia tensor of bodies
    mI1 = mBody1->getInertiaTensorInverseWorld();
    mI2 = mBody2->getInertiaTensorInverseWorld();

    // Compute the vector from body center to the anchor point in world-space
    mR1World = orientationBody1 * mLocalAnchorPointBody1;
    mR2World = orientationBody2 * mLocalAnchorPointBody2;

    // Compute the corresponding skew-symmetric matrices
    Matrix3x3 skewSymmetricMatrixU1= Matrix3x3::computeSkewSymmetricMatrixForCrossProduct(mR1World);
    Matrix3x3 skewSymmetricMatrixU2= Matrix3x3::computeSkewSymmetricMatrixForCrossProduct(mR2World);

    // Compute the matrix K=JM^-1J^t (3x3 matrix)
    decimal inverseMassBodies = 0.0;
    if (mBody1->isMotionEnabled()) {
        inverseMassBodies += mBody1->getMassInverse();
    }
    if (mBody2->isMotionEnabled()) {
        inverseMassBodies += mBody2->getMassInverse();
    }
    Matrix3x3 massMatrix = Matrix3x3(inverseMassBodies, 0, 0,
                                    0, inverseMassBodies, 0,
                                    0, 0, inverseMassBodies);
    if (mBody1->isMotionEnabled()) {
        massMatrix += skewSymmetricMatrixU1 * mI1 * skewSymmetricMatrixU1.getTranspose();
    }
    if (mBody2->isMotionEnabled()) {
        massMatrix += skewSymmetricMatrixU2 * mI2 * skewSymmetricMatrixU2.getTranspose();
    }

    // Compute the inverse mass matrix K^-1
    mInverseMassMatrix.setToZero();
    if (mBody1->isMotionEnabled() || mBody2->isMotionEnabled()) {
        mInverseMassMatrix = massMatrix.getInverse();
    }

    // Compute the bias "b" of the constraint
    mBiasVector.setToZero();
    if (mPositionCorrectionTechnique == BAUMGARTE_JOINTS) {
        decimal biasFactor = (BETA / constraintSolverData.timeStep);
        mBiasVector = biasFactor * (x2 + mR2World - x1 - mR1World);
    }

    // If warm-starting is not enabled
    if (!constraintSolverData.isWarmStartingActive) {

        // Reset the accumulated impulse
        mImpulse.setToZero();
    }
}

// Warm start the constraint (apply the previous impulse at the beginning of the step)
void BallAndSocketJoint::warmstart(const ConstraintSolverData& constraintSolverData) {

    // Get the velocities
    Vector3& v1 = constraintSolverData.linearVelocities[mIndexBody1];
    Vector3& v2 = constraintSolverData.linearVelocities[mIndexBody2];
    Vector3& w1 = constraintSolverData.angularVelocities[mIndexBody1];
    Vector3& w2 = constraintSolverData.angularVelocities[mIndexBody2];

    // Get the inverse mass of the bodies
    const decimal inverseMassBody1 = mBody1->getMassInverse();
    const decimal inverseMassBody2 = mBody2->getMassInverse();

    if (mBody1->isMotionEnabled()) {

        // Compute the impulse P=J^T * lambda
        const Vector3 linearImpulseBody1 = -mImpulse;
        const Vector3 angularImpulseBody1 = mImpulse.cross(mR1World);

        // Apply the impulse to the body
        v1 += inverseMassBody1 * linearImpulseBody1;
        w1 += mI1 * angularImpulseBody1;
    }
    if (mBody2->isMotionEnabled()) {

        // Compute the impulse P=J^T * lambda
        const Vector3 linearImpulseBody2 = mImpulse;
        const Vector3 angularImpulseBody2 = -mImpulse.cross(mR2World);

        // Apply the impulse to the body
        v2 += inverseMassBody2 * linearImpulseBody2;
        w2 += mI2 * angularImpulseBody2;
    }
}

// Solve the velocity constraint
void BallAndSocketJoint::solveVelocityConstraint(const ConstraintSolverData& constraintSolverData) {

    // Get the velocities
    Vector3& v1 = constraintSolverData.linearVelocities[mIndexBody1];
    Vector3& v2 = constraintSolverData.linearVelocities[mIndexBody2];
    Vector3& w1 = constraintSolverData.angularVelocities[mIndexBody1];
    Vector3& w2 = constraintSolverData.angularVelocities[mIndexBody2];

    // Get the inverse mass of the bodies
    decimal inverseMassBody1 = mBody1->getMassInverse();
    decimal inverseMassBody2 = mBody2->getMassInverse();

    // Compute J*v
    const Vector3 Jv = v2 + w2.cross(mR2World) - v1 - w1.cross(mR1World);

    // Compute the Lagrange multiplier lambda
    const Vector3 deltaLambda = mInverseMassMatrix * (-Jv - mBiasVector);
    mImpulse += deltaLambda;

    if (mBody1->isMotionEnabled()) {

        // Compute the impulse P=J^T * lambda
        const Vector3 linearImpulseBody1 = -deltaLambda;
        const Vector3 angularImpulseBody1 = deltaLambda.cross(mR1World);

        // Apply the impulse to the body
        v1 += inverseMassBody1 * linearImpulseBody1;
        w1 += mI1 * angularImpulseBody1;
    }
    if (mBody2->isMotionEnabled()) {

        // Compute the impulse P=J^T * lambda
        const Vector3 linearImpulseBody2 = deltaLambda;
        const Vector3 angularImpulseBody2 = -deltaLambda.cross(mR2World);

        // Apply the impulse to the body
        v2 += inverseMassBody2 * linearImpulseBody2;
        w2 += mI2 * angularImpulseBody2;
    }
}

// Solve the position constraint (for position error correction)
void BallAndSocketJoint::solvePositionConstraint(const ConstraintSolverData& constraintSolverData) {

    // If the error position correction technique is not the non-linear-gauss-seidel, we do
    // do not execute this method
    if (mPositionCorrectionTechnique != NON_LINEAR_GAUSS_SEIDEL) return;

    // Get the bodies positions and orientations
    Vector3& x1 = constraintSolverData.positions[mIndexBody1];
    Vector3& x2 = constraintSolverData.positions[mIndexBody2];
    Quaternion& q1 = constraintSolverData.orientations[mIndexBody1];
    Quaternion& q2 = constraintSolverData.orientations[mIndexBody2];

    // Get the inverse mass and inverse inertia tensors of the bodies
    decimal inverseMassBody1 = mBody1->getMassInverse();
    decimal inverseMassBody2 = mBody2->getMassInverse();

    // Recompute the inverse inertia tensors
    mI1 = mBody1->getInertiaTensorInverseWorld();
    mI2 = mBody2->getInertiaTensorInverseWorld();

    // Compute the vector from body center to the anchor point in world-space
    mR1World = q1 * mLocalAnchorPointBody1;
    mR2World = q2 * mLocalAnchorPointBody2;

    // Compute the corresponding skew-symmetric matrices
    Matrix3x3 skewSymmetricMatrixU1= Matrix3x3::computeSkewSymmetricMatrixForCrossProduct(mR1World);
    Matrix3x3 skewSymmetricMatrixU2= Matrix3x3::computeSkewSymmetricMatrixForCrossProduct(mR2World);

    // Recompute the inverse mass matrix K=J^TM^-1J of of the 3 translation constraints
    decimal inverseMassBodies = 0.0;
    if (mBody1->isMotionEnabled()) {
        inverseMassBodies += inverseMassBody1;
    }
    if (mBody2->isMotionEnabled()) {
        inverseMassBodies += inverseMassBody2;
    }
    Matrix3x3 massMatrix = Matrix3x3(inverseMassBodies, 0, 0,
                                    0, inverseMassBodies, 0,
                                    0, 0, inverseMassBodies);
    if (mBody1->isMotionEnabled()) {
        massMatrix += skewSymmetricMatrixU1 * mI1 * skewSymmetricMatrixU1.getTranspose();
    }
    if (mBody2->isMotionEnabled()) {
        massMatrix += skewSymmetricMatrixU2 * mI2 * skewSymmetricMatrixU2.getTranspose();
    }
    mInverseMassMatrix.setToZero();
    if (mBody1->isMotionEnabled() || mBody2->isMotionEnabled()) {
        mInverseMassMatrix = massMatrix.getInverse();
    }

    // Compute the constraint error (value of the C(x) function)
    const Vector3 constraintError = (x2 + mR2World - x1 - mR1World);

    // Compute the Lagrange multiplier lambda
    // TODO : Do not solve the system by computing the inverse each time and multiplying with the
    //        right-hand side vector but instead use a method to directly solve the linear system.
    const Vector3 lambda = mInverseMassMatrix * (-constraintError);

    // Apply the impulse to the bodies of the joint (directly update the position/orientation)
    if (mBody1->isMotionEnabled()) {

        // Compute the impulse
        const Vector3 linearImpulseBody1 = -lambda;
        const Vector3 angularImpulseBody1 = lambda.cross(mR1World);

        // Compute the pseudo velocity
        const Vector3 v1 = inverseMassBody1 * linearImpulseBody1;
        const Vector3 w1 = mI1 * angularImpulseBody1;

        // Update the body position/orientation
        x1 += v1;
        q1 += Quaternion(0, w1) * q1 * decimal(0.5);
        q1.normalize();
    }
    if (mBody2->isMotionEnabled()) {

        // Compute the impulse
        const Vector3 linearImpulseBody2 = lambda;
        const Vector3 angularImpulseBody2 = -lambda.cross(mR2World);

        // Compute the pseudo velocity
        const Vector3 v2 = inverseMassBody2 * linearImpulseBody2;
        const Vector3 w2 = mI2 * angularImpulseBody2;

        // Update the body position/orientation
        x2 += v2;
        q2 += Quaternion(0, w2) * q2 * decimal(0.5);
        q2.normalize();
    }
}

