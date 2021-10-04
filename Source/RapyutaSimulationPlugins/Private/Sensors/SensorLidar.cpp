// Copyright 2020-2021 Rapyuta Robotics Co., Ltd.

#include "Sensors/SensorLidar.h"

#include "Components/LineBatchComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include <limits>

DEFINE_LOG_CATEGORY(LogROS2Sensor);

// Sets default values
ASensorLidar::ASensorLidar() : ABaseLidar()
{
    LidarPublisher = CreateDefaultSubobject<UROS2Publisher>(TEXT("LidarPublisher"));
    LidarPublisher->TopicName = TEXT("scan");
    LidarPublisher->PublicationFrequencyHz = ScanFrequency;
    LidarPublisher->MsgClass = UROS2LaserScanMsg::StaticClass();
}

void ASensorLidar::LidarMessageUpdate(UROS2GenericMsg* TopicMessage)
{
    UROS2LaserScanMsg* ScanMessage = Cast<UROS2LaserScanMsg>(TopicMessage);
    ScanMessage->SetMsg(GetROS2Data());
}

// Called every frame
void ASensorLidar::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
#if TRACE_ASYNC
    UWorld* world = GetWorld();
    for (auto i = 0; i < NSamplesPerScan; ++i)
    {
        if (TraceHandles[i]._Data.FrameNumber != 0)
        {
            FTraceDatum Output;

            if (world->QueryTraceData(TraceHandles[i], Output))
            {
                if (Output.OutHits.Num() > 0)
                {
                    check(Output.OutHits.Num() > 0);
                    check(i < RecordedHits.Num());
                    check(i < TraceHandles.Num());
                    TraceHandles[i]._Data.FrameNumber = 0;
                    // We should only be tracing the first hit anyhow
                    RecordedHits[i] = Output.OutHits[0];
                }
                else
                {
                    TraceHandles[i]._Data.FrameNumber = 0;
                    RecordedHits[i] = FHitResult();
                    RecordedHits[i].TraceStart = Output.Start;
                    RecordedHits[i].TraceEnd = Output.End;
                }
            }
        }
    }
#endif
}

void ASensorLidar::Run()
{
    RecordedHits.Empty();
    RecordedHits.Init(FHitResult(ForceInit), NSamplesPerScan);

#if TRACE_ASYNC
    TraceHandles.Empty();
    TraceHandles.Init(FTraceHandle{}, NSamplesPerScan);
#endif

    GetWorld()->GetGameInstance()->GetTimerManager().SetTimer(
        TimerHandle, this, &ASensorLidar::Scan, 1.f / static_cast<float>(ScanFrequency), true);
    IsInitialized = true;
}

void ASensorLidar::Scan()
{
    DHAngle = FOVHorizontal / static_cast<float>(NSamplesPerScan);

    // complex collisions: true
    FCollisionQueryParams TraceParams = FCollisionQueryParams(FName(TEXT("Laser_Trace")), true, this);
    TraceParams.bReturnPhysicalMaterial = true;

    // TraceParams.bIgnoreTouches = true;
    TraceParams.bTraceComplex = true;
    TraceParams.bReturnFaceIndex = true;

    FVector lidarPos = GetActorLocation();
    FRotator lidarRot = GetActorRotation();

#if TRACE_ASYNC
    // This is cheesy, but basically if the first trace is in flight we assume they're all waiting and don't do another trace.
    // This is not good if done on other threads and only works because both timers and actor ticks happen on the game thread.
    if (TraceHandles[0]._Data.FrameNumber == 0)
    {
        UWorld* world = GetWorld();
        for (auto i = 0; i < NSamplesPerScan; ++i)
        {
            const float HAngle = StartAngle + DHAngle * i;

            FRotator laserRot(0, HAngle, 0);
            FRotator rot = UKismetMathLibrary::ComposeRotators(laserRot, lidarRot);

            FVector startPos = lidarPos + MinRange * UKismetMathLibrary::GetForwardVector(rot);
            FVector endPos = lidarPos + MaxRange * UKismetMathLibrary::GetForwardVector(rot);
            // To be considered: += WithNoise * FVector(GaussianRNGPosition(Gen),GaussianRNGPosition(Gen),GaussianRNGPosition(Gen));

            TraceHandles[i] = world->AsyncLineTraceByChannel(EAsyncTraceType::Single,
                                                             startPos,
                                                             endPos,
                                                             ECC_Visibility,
                                                             TraceParams,
                                                             FCollisionResponseParams::DefaultResponseParam,
                                                             nullptr);
        }
    }
#else
    ParallelFor(
        NSamplesPerScan,
        [this, &TraceParams, &lidarPos, &lidarRot](int32 Index)
        {
            const float HAngle = StartAngle + DHAngle * Index;

            FRotator laserRot(0, HAngle, 0);
            FRotator rot = UKismetMathLibrary::ComposeRotators(laserRot, lidarRot);

            FVector startPos = lidarPos + MinRange * UKismetMathLibrary::GetForwardVector(rot);
            FVector endPos = lidarPos + MaxRange * UKismetMathLibrary::GetForwardVector(rot);
            // + WithNoise *  FVector(GaussianRNGPosition(Gen),GaussianRNGPosition(Gen),GaussianRNGPosition(Gen));

            GetWorld()->LineTraceSingleByChannel(
                RecordedHits[Index], startPos, endPos, ECC_Visibility, TraceParams, FCollisionResponseParams::DefaultResponseParam);
        },
        false);
#endif

    if (WithNoise)
    {
        // this approach to noise is different from the above:
        // noise on the linetrace input means that the further the hit, the larger the error, while here the error is independent
        // from distance
        ParallelFor(
            NSamplesPerScan,
            [this, &TraceParams, &lidarPos, &lidarRot](int32 Index)
            {
                RecordedHits[Index].ImpactPoint +=
                    FVector(GaussianRNGPosition(Gen), GaussianRNGPosition(Gen), GaussianRNGPosition(Gen));
                RecordedHits[Index].TraceEnd +=
                    FVector(GaussianRNGPosition(Gen), GaussianRNGPosition(Gen), GaussianRNGPosition(Gen));
            },
            false);
    }

    TimeOfLastScan = UGameplayStatics::GetTimeSeconds(GetWorld());
    dt = 1.f / static_cast<float>(ScanFrequency);

    // need to store on a structure associating hits with time?
    // GetROS2Data needs to get all data since the last Get? or the last within the last time interval?

    ULineBatchComponent* const LineBatcher = GetWorld()->PersistentLineBatcher;
    if (LineBatcher != nullptr && ShowLidarRays)
    {
        for (auto& h : RecordedHits)
        {
            if (h.Actor != nullptr)
            {
                float Distance = (MinRange * (h.Distance > 0) + h.Distance) * .01f;
                if (h.PhysMaterial != nullptr)
                {
                    // retroreflective material
                    if (h.PhysMaterial->SurfaceType == EPhysicalSurface::SurfaceType1)
                    {
                        // UE_LOG(LogTemp, Warning, TEXT("retroreflective surface type hit"));
                        // LineBatcher->DrawLine(h.TraceStart, h.ImpactPoint, ColorReflected, 10, .5, dt);
                        LineBatcher->DrawPoint(
                            h.ImpactPoint, GetColorFromIntensity(IntensityFromDist(IntensityReflective, Distance)), 5, 10, dt);
                    }
                    // non reflective material
                    else if (h.PhysMaterial->SurfaceType == EPhysicalSurface::SurfaceType_Default)
                    {
                        // UE_LOG(LogTemp, Warning, TEXT("default surface type hit"));
                        // LineBatcher->DrawLine(h.TraceStart, h.ImpactPoint, ColorHit, 10, .5, dt);
                        LineBatcher->DrawPoint(
                            h.ImpactPoint, GetColorFromIntensity(IntensityFromDist(IntensityNonReflective, Distance)), 5, 10, dt);
                    }
                    // reflective material
                    else if (h.PhysMaterial->SurfaceType == EPhysicalSurface::SurfaceType2)
                    {
                        FVector HitSurfaceNormal = h.Normal;
                        FVector RayDirection = h.TraceEnd - h.TraceStart;
                        RayDirection.Normalize();

                        float NormalAlignment = FVector::DotProduct(HitSurfaceNormal, -RayDirection);
                        NormalAlignment *= NormalAlignment;
                        NormalAlignment *= NormalAlignment;
                        NormalAlignment *= NormalAlignment;
                        NormalAlignment *= NormalAlignment;
                        NormalAlignment *= NormalAlignment;    // pow 32
                        // LineBatcher->DrawLine(h.TraceStart, h.ImpactPoint, FLinearColor::LerpUsingHSV(ColorHit, ColorReflected,
                        // NormalAlignment), 10, .5, dt); NormalAlignment =
                        // (NormalAlignment*(IntensityReflective-IntensityNonReflective) + IntensityNonReflective)/IntensityMax;
                        // LineBatcher->DrawPoint(h.ImpactPoint, InterpolateColor(NormalAlignment), 5, 10, dt);
                        LineBatcher->DrawPoint(
                            h.ImpactPoint,
                            GetColorFromIntensity(IntensityFromDist(
                                NormalAlignment * (IntensityReflective - IntensityNonReflective) + IntensityNonReflective,
                                Distance)),
                            5,
                            10,
                            dt);
                    }
                }
                else
                {
                    // UE_LOG(LogTemp, Warning, TEXT("no physics material"));
                    // LineBatcher->DrawLine(h.TraceStart, h.ImpactPoint, ColorHit, 10, .5, dt);
                    LineBatcher->DrawPoint(
                        h.ImpactPoint, GetColorFromIntensity(IntensityFromDist(IntensityNonReflective, Distance)), 5, 10, dt);
                }
            }
            else if (ShowLidarRayMisses)
            {
                // LineBatcher->DrawLine(h.TraceStart, h.TraceEnd, ColorMiss, 10, .25, dt);
                LineBatcher->DrawPoint(h.TraceEnd, ColorMiss, 2.5, 10, dt);
            }
        }
    }
}

bool ASensorLidar::Visible(AActor* TargetActor)
{
    TArray<FHitResult> RecordedVizHits;
    RecordedVizHits.Init(FHitResult(ForceInit), NSamplesPerScan);

    DHAngle = FOVHorizontal / static_cast<float>(NSamplesPerScan);

    // complex collisions: true
    FCollisionQueryParams TraceParams = FCollisionQueryParams(FName(TEXT("Laser_Trace")), true, this);
    TraceParams.bReturnPhysicalMaterial = true;
    // TraceParams.bIgnoreTouches = true;
    TraceParams.bTraceComplex = true;
    TraceParams.bReturnFaceIndex = true;

    FVector lidarPos = GetActorLocation();
    FRotator lidarRot = GetActorRotation();

    ParallelFor(
        NSamplesPerScan,
        [this, &TraceParams, &lidarPos, &lidarRot, &RecordedVizHits](int32 Index)
        {
            const float HAngle = StartAngle + DHAngle * Index;

            FRotator laserRot(0, HAngle, 0);
            FRotator rot = UKismetMathLibrary::ComposeRotators(laserRot, lidarRot);

            FVector startPos = lidarPos + MinRange * UKismetMathLibrary::GetForwardVector(rot);
            FVector endPos = lidarPos + MaxRange * UKismetMathLibrary::GetForwardVector(rot);
            // To be considered: + WithNoise * FVector(GaussianRNGPosition(Gen),GaussianRNGPosition(Gen),GaussianRNGPosition(Gen));

            GetWorld()->LineTraceSingleByChannel(RecordedVizHits[Index],
                                                 startPos,
                                                 endPos,
                                                 ECC_Visibility,
                                                 TraceParams,
                                                 FCollisionResponseParams::DefaultResponseParam);
        },
        false);

    for (auto& h : RecordedVizHits)
    {
        if (h.Actor == TargetActor)
        {
            return true;
        }
    }
    return false;
}

void ASensorLidar::InitLidar(AROS2Node* Node, const FString& TopicName)
{
    Super::InitLidar(Node,TopicName);

    Run();
}

void ASensorLidar::InitToNode(AROS2Node* Node)
{
    if (IsValid(Node))
    {
        check(IsValid(LidarPublisher));

        LidarPublisher->UpdateDelegate.BindDynamic(this, &ASensorLidar::LidarMessageUpdate);
        Node->AddPublisher(LidarPublisher);
        LidarPublisher->Init(UROS2QoS::SensorData);
    }
}

float ASensorLidar::GetMinAngleRadians() const
{
    return FMath::DegreesToRadians(-StartAngle - FOVHorizontal);
}

float ASensorLidar::GetMaxAngleRadians() const
{
    return FMath::DegreesToRadians(-StartAngle);
}

FROSLaserScan ASensorLidar::GetROS2Data()
{
    FROSLaserScan retValue;
    retValue.header_stamp_sec = (int32)TimeOfLastScan;
    uint64 ns = (uint64)(TimeOfLastScan * 1e+09f);
    retValue.header_stamp_nanosec = (uint32)(ns - (retValue.header_stamp_sec * 1e+09));

    retValue.header_frame_id = FrameId;

    retValue.angle_min = GetMinAngleRadians();
    retValue.angle_max = GetMaxAngleRadians();
    retValue.angle_increment = FMath::DegreesToRadians(DHAngle);
    retValue.time_increment = dt / NSamplesPerScan;
    retValue.scan_time = dt;
    retValue.range_min = MinRange * .01f;
    retValue.range_max = MaxRange * .01f;

    retValue.ranges.Empty();
    retValue.intensities.Empty();
    // note that angles are reversed compared to rviz
    // ROS is right handed
    // UE4 is left handed
    for (auto i = 0; i < RecordedHits.Num(); i++)
    {
        // convert to [m]
        retValue.ranges.Add((MinRange * (RecordedHits.Last(i).Distance > 0) + RecordedHits.Last(i).Distance) * .01f);

        const float IntensityScale = 1.f + WithNoise * GaussianRNGIntensity(Gen);

        UStaticMeshComponent* ComponentHit = Cast<UStaticMeshComponent>(RecordedHits.Last(i).GetComponent());
        if (RecordedHits.Last(i).PhysMaterial != nullptr)
        {
            // retroreflective material
            if (RecordedHits.Last(i).PhysMaterial->SurfaceType == EPhysicalSurface::SurfaceType1)
            {
                retValue.intensities.Add(IntensityScale * IntensityReflective);
            }
            // non-reflective material
            else if (RecordedHits.Last(i).PhysMaterial->SurfaceType == EPhysicalSurface::SurfaceType_Default)
            {
                retValue.intensities.Add(IntensityScale * IntensityNonReflective);
            }
            // reflective material
            else if (RecordedHits.Last(i).PhysMaterial->SurfaceType == EPhysicalSurface::SurfaceType2)
            {
                FVector HitSurfaceNormal = RecordedHits.Last(i).Normal;
                FVector RayDirection = RecordedHits.Last(i).TraceEnd - RecordedHits.Last(i).TraceStart;
                RayDirection.Normalize();

                // the dot product for this should always be between 0 and 1
                const float Intensity =
                    FMath::Clamp(IntensityNonReflective + (IntensityReflective - IntensityNonReflective) *
                                                              FVector::DotProduct(HitSurfaceNormal, -RayDirection),
                                 IntensityNonReflective,
                                 IntensityReflective);
                check(Intensity >= IntensityNonReflective);
                check(Intensity <= IntensityReflective);
                retValue.intensities.Add(IntensityScale * Intensity);
            }
        }
        else
        {
            retValue.intensities.Add(std::numeric_limits<double>::quiet_NaN());
        }
    }

    return retValue;
}