/**
 * @file JointComponent.h
 * @brief Base Joint component class which is used as part of #ARobotVehicle.
 * @copyright Copyright 2020-2022 Rapyuta Robotics Co., Ltd.
 */

#pragma once

#include "Components/ActorComponent.h"
#include "Core/RRStaticMeshComponent.h"
#include "CoreMinimal.h"

#include "RRJointComponent.generated.h"

#define RAPYUTA_JOINT_DEBUG (0)

UENUM(BlueprintType)
enum class ERRJointControlType : uint8
{
    POSITION UMETA(DisplayName = "Position"),
    VELOCITY UMETA(DisplayName = "Velocity"),
    EFFORT UMETA(DisplayName = "Effort")
};

/**
 * @brief Base Joints class. Other joint class should inherit from this class.
 * 6 DOF joints class with velocity or position control.
 * temporary implementation of joints. should be merged with RobotImporter later.
 *
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAPYUTASIMULATIONPLUGINS_API URRJointComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    URRJointComponent();

protected:
    virtual void BeginPlay() override;
    virtual void PoseFromArray(const TArray<float>& InPose, FVector& OutPosition, FRotator& OutOrientation);
    virtual void VelocityFromArray(const TArray<float>& InVelocity, FVector& OutLinearVelocity, FVector& OutAngularVelocity);

public:
    virtual bool IsValid();

    /**
     * @brief Initialize #JointToChildLink and #ParentLinkToJoint
     * 
     */
    virtual void Initialize();

    /**
     * @brief Directly set velocity.
     * Control to move joint with this velocity should be implemented in child class.
     * @param InLinearVelocity
     * @param InAngularVelocity
     */
    UFUNCTION(BlueprintCallable)
    virtual void SetVelocity(const FVector& InLinearVelocity, const FVector& InAngularVelocity);

    /**
     * @brief Set velocity target
     * Control to move joint with this velocity should be implemented in child class.
     * @param InLinearVelocity
     * @param InAngularVelocity
     */
    UFUNCTION(BlueprintCallable)
    virtual void SetVelocityTarget(const FVector& InLinearVelocity, const FVector& InAngularVelocity);

    /**
     * @brief Set the Velocity With TArray.
     * Control to move joint with this velocity should be implemented in child class.
     * TArray size should be #LinearDOF +  #RotationalDOF
     * @param InVelocity
     */
    UFUNCTION(BlueprintCallable)
    virtual void SetVelocityWithArray(const TArray<float>& InVelocity);

    /**
     * @brief Set the Velocity Target.
     * Control to move joint with this velocity should be implemented in child class.
     * TArray size should be #LinearDOF +  #RotationalDOF
     * @param InVelocity
     */
    UFUNCTION(BlueprintCallable)
    virtual void SetVelocityTargetWithArray(const TArray<float>& InVelocity);

    /**
     * @brief Check Pose reach the target pose.
     *
     */
    UFUNCTION(BlueprintCallable)
    virtual bool HasReachedVelocityTarget(const float InLinearTolerance, const float InAngularTolerance);

    /**
     * @brief Directly set pose.
     * Control to teleport joint to this pose should be implemented in child class.
     * @param InPosition
     * @param InOrientation
     */
    UFUNCTION(BlueprintCallable)
    virtual void SetPose(const FVector& InPosition, const FRotator& InOrientation);

    /**
     * @brief Set Pose Target.
     * Control to move joint to pose target should be implemented in child class.
     * @param InPosition
     * @param InOrientation
     */
    UFUNCTION(BlueprintCallable)
    virtual void SetPoseTarget(const FVector& InPosition, const FRotator& InOrientation);

    /**
     * @brief Check Pose reach the target pose.
     *
     */
    UFUNCTION(BlueprintCallable)
    virtual bool HasReachedPoseTarget(const float InPositionTolerance, const float InOrientationTolerance);

    /**
     * @brief Directly set pose.
     * Control to teleport joint to pose should be implemented in child class.
     * TArray size should be #LinearDOF +  #RotationalDOF
     * @param InPose
     */
    UFUNCTION(BlueprintCallable)
    virtual void SetPoseWithArray(const TArray<float>& InPose);

    /**
     * @brief Set Pose Target.
     * Control to move joint to pose target should be implemented in child class.
     * TArray size should be #LinearDOF +  #RotationalDOF
     * @param InPose
     */
    UFUNCTION(BlueprintCallable)
    virtual void SetPoseTargetWithArray(const TArray<float>& InPose);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector LinearVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector AngularVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector LinearVelocityTarget = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector AngularVelocityTarget = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector PositionTarget = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator OrientationTarget = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Orientation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMeshComponent* ParentLink;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMeshComponent* ChildLink;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ERRJointControlType ControlType = ERRJointControlType::POSITION;

    //! Linear Degrees Of Freedom
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 LinearDOF = 0;

    //! Rotational Degrees Of Freedom
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 RotationalDOF = 1;

    //! Pose Limitations[cm]
    //! @todo is it possible to set inf?
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector PositionMax = FVector(1000, 1000, 1000);

    //! Pose Limitations[cm]
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector PositionMin = FVector(-1000, -1000, -1000);

    //! Orientation Limitations[deg]
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator OrientationMax = FRotator(180.f);

    //! Orientation Limitations[deg]
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator OrientationMin = FRotator(-180, -180, -180);

    //! Linear Velocity Limitations[cm/s]
    //! @todo is it possible to set inf?
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector LinearVelMax = FVector(1000, 1000, 1000);

    //! Angular Velocity Limitations[deg/s]
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector AngularVelMax = FVector(180.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bLimitRoll = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bLimitPitch = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bLimitYaw = true;

protected:

    UPROPERTY()
    FTransform JointToChildLink = FTransform::Identity;
    UPROPERTY()
    FTransform ParentLinkToJoint = FTransform::Identity;
};
