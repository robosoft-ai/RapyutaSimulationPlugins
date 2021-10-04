// Copyright 2020-2021 Rapyuta Robotics Co., Ltd.

#pragma once

// UE
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

// rclUE
#include "Msgs/ROS2LaserScanMsg.h"
#include "BaseLidar.h"

#include "SensorLidar.generated.h"

UCLASS(ClassGroup = (Custom), Blueprintable, meta = (BlueprintSpawnableComponent))
class RAPYUTASIMULATIONPLUGINS_API ASensorLidar : public ABaseLidar
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ASensorLidar();

    // Called every frame
    virtual void Tick(float DeltaTime) override;
    
    void Run() override;
    
    void Scan() override;

    void LidarMessageUpdate(UROS2GenericMsg* TopicMessage) override;
    
    bool Visible(AActor* TargetActor) override;

    void InitLidar(AROS2Node* Node, const FString& TopicName) override;

    void InitToNode(AROS2Node* Node) override;

    // this should probably be removed so that the sensor can be decoupled from the message types
    UFUNCTION(BlueprintCallable)
    FROSLaserScan GetROS2Data();

    UFUNCTION(BlueprintCallable)
    float GetMinAngleRadians() const;

    UFUNCTION(BlueprintCallable)
    float GetMaxAngleRadians() const;
};