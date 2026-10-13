// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXDeviceA.h"
#include "TEAMBOXEnergyCore.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ATEAMBOXDeviceA::ATEAMBOXDeviceA()
{
	PrimaryActorTick.bCanEverTick = true;

	// Initialize default values
	MaxBeamRange = 10000.0f; // 100 meters in cm
	TraceChannel = ECC_Visibility;
	bEnableDebugVisualization = false;
	bUseLocalSpaceForBeam = false; // Default to world space

	// Receive-to-activate defaults
	bInitiallyEmitting = true;   // 保持原有行为：默认一开始就发射
	bOneShotActivate = true;    // 默认：接收一次即永久激活
	ReceiverComponent = nullptr;
	bIsActivated = true;
	bHasEverReceivedBeam = false;
	ActivationSFX = nullptr;

	// Create Arrow component as root
	ArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent"));
	RootComponent = ArrowComponent;
	ArrowComponent->SetArrowColor(FLinearColor::Red);
	ArrowComponent->SetArrowSize(2.0f);

	// Create Niagara Beam component
	BeamComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BeamComponent"));
	BeamComponent->SetupAttachment(RootComponent);
}

void ATEAMBOXDeviceA::BeginPlay()
{
	Super::BeginPlay();
	
	// Initialize beam locations
	BeamStartLocation = GetActorLocation();
	BeamEndLocation = GetActorLocation();

	// 初始是否发射由 bInitiallyEmitting 决定；非初始发射时依赖接收光线激活
	bIsActivated = bInitiallyEmitting;
	bHasEverReceivedBeam = false;

	if (BeamComponent)
	{
		BeamComponent->SetVisibility(bIsActivated);
	}

	// [DeviceA 接收] 启动时打印一次，便于确认配置与接收器是否设置
	UE_LOG(LogTemp, Log, TEXT("[DeviceA Receive] %s BeginPlay: bInitiallyEmitting=%d, bOneShotActivate=%d, bIsActivated=%d, ReceiverComponent=%s"),
		*GetName(), bInitiallyEmitting, bOneShotActivate, bIsActivated,
		ReceiverComponent ? *ReceiverComponent->GetName() : TEXT("null (未设置，请在 Blueprint 中调用 SetReceiverComponent)"));
}

void ATEAMBOXDeviceA::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 未激活时：每帧检测以判断是否被照到并激活。
	// 已激活且为“持续接收”模式时：也要每帧检测，以便光线移开时关闭激活。
	if (!bIsActivated || !bOneShotActivate)
	{
		UpdateReceiveState();
	}

	if (bIsActivated)
	{
		UpdateBeam();
		if (BeamComponent)
		{
			BeamComponent->SetVisibility(true);
		}
	}
	else
	{
		if (BeamComponent)
		{
			BeamComponent->SetVisibility(false);
		}
	}
}

void ATEAMBOXDeviceA::UpdateReceiveState()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const bool bWasActiveBeforeCheck = bIsActivated;

	// 未设置接收器时每约 2 秒提醒一次，便于排查“接受到光线没有激活”
	static int32 LogSkipCount = 0;
	if (ReceiverComponent == nullptr)
	{
		if ((LogSkipCount++ % 120) == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DeviceA Receive] %s ReceiverComponent 为 null，无法检测光线。请在 Blueprint 中创建 Box 并调用 SetReceiverComponent(Box)。"), *GetName());
		}
		return;
	}

	bool bReceivingThisFrame = false;
	FString ReceiveSourceDesc; // 用于 log：光线来自哪个 Actor

	// 检测所有 DeviceA 的光线是否照到本机接收器
	TArray<AActor*> DeviceAActors;
	UGameplayStatics::GetAllActorsOfClass(World, ATEAMBOXDeviceA::StaticClass(), DeviceAActors);

	for (AActor* Actor : DeviceAActors)
	{
		ATEAMBOXDeviceA* Other = Cast<ATEAMBOXDeviceA>(Actor);
		if (Other == nullptr || Other == this || !Other->IsActivated() || Other->ArrowComponent == nullptr)
		{
			continue;
		}

		FVector Start = Other->ArrowComponent->GetComponentLocation();
		FVector Forward = Other->ArrowComponent->GetForwardVector();
		FVector End = Start + (Forward * Other->MaxBeamRange);

		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Other);
		QueryParams.bTraceComplex = true;

		bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, Other->TraceChannel, QueryParams);
		UPrimitiveComponent* HitComp = HitResult.GetComponent();
		// 直接命中接收器 Box，或命中本 Actor 且命中点在接收器包围盒内（照到接收区域算激活，只打到底座等不算）
		bool bHitReceiver = (HitComp == ReceiverComponent);
		bool bHitThisAndInReceiverBounds = (HitResult.GetActor() == this) && ReceiverComponent &&
			ReceiverComponent->CalcBounds(ReceiverComponent->GetComponentTransform()).GetBox().ExpandBy(10.0f).IsInsideOrOn(HitResult.Location);
		bool bIsOurReceiver = bHit && HitResult.bBlockingHit && (bHitReceiver || bHitThisAndInReceiverBounds);

		if (bIsOurReceiver)
		{
			bReceivingThisFrame = true;
			ReceiveSourceDesc = FString::Printf(TEXT("DeviceA:%s"), Other ? *Other->GetName() : TEXT("?"));
			break;
		}
	}

	// 检测所有正在发射的 EnergyCore 的光线是否照到本机接收器
	if (!bReceivingThisFrame)
	{
		TArray<AActor*> EnergyCoreActors;
		UGameplayStatics::GetAllActorsOfClass(World, ATEAMBOXEnergyCore::StaticClass(), EnergyCoreActors);

		for (AActor* Actor : EnergyCoreActors)
		{
			ATEAMBOXEnergyCore* Core = Cast<ATEAMBOXEnergyCore>(Actor);
			if (Core == nullptr || !Core->IsReceivingBeam() || Core->GetBeamStartComponent() == nullptr)
			{
				continue;
			}

			FVector Start = Core->GetBeamStartComponent()->GetComponentLocation();
			FVector Forward = Core->GetBeamStartComponent()->GetForwardVector();
			FVector End = Start + (Forward * Core->MaxBeamRange);

			FHitResult HitResult;
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(Core);
			QueryParams.bTraceComplex = true;

			bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, Core->TraceChannel, QueryParams);
			UPrimitiveComponent* HitComp = HitResult.GetComponent();
			bool bHitReceiver = (HitComp == ReceiverComponent);
			bool bHitThisAndInReceiverBounds = (HitResult.GetActor() == this) && ReceiverComponent &&
				ReceiverComponent->CalcBounds(ReceiverComponent->GetComponentTransform()).GetBox().ExpandBy(10.0f).IsInsideOrOn(HitResult.Location);
			bool bIsOurReceiver = bHit && HitResult.bBlockingHit && (bHitReceiver || bHitThisAndInReceiverBounds);

			if (bIsOurReceiver)
			{
				bReceivingThisFrame = true;
				ReceiveSourceDesc = FString::Printf(TEXT("EnergyCore:%s"), Core ? *Core->GetName() : TEXT("?"));
				break;
			}
		}
	}

	// 约每 2 秒在未激活时打印一次检测概况（便于确认是否有光源、射线是否打到别的物体）
	static int32 DiagFrameCount = 0;
	const bool bPeriodicLog = (DiagFrameCount % 120) == 0;
	DiagFrameCount++;

	if (bPeriodicLog && !bReceivingThisFrame)
	{
		int32 NumActiveDeviceA = 0;
		for (AActor* A : DeviceAActors)
		{
			ATEAMBOXDeviceA* D = Cast<ATEAMBOXDeviceA>(A);
			if (D && D != this && D->IsActivated()) NumActiveDeviceA++;
		}
		TArray<AActor*> EnergyCoreActors;
		UGameplayStatics::GetAllActorsOfClass(World, ATEAMBOXEnergyCore::StaticClass(), EnergyCoreActors);
		int32 NumEmittingCores = 0;
		for (AActor* A : EnergyCoreActors)
		{
			ATEAMBOXEnergyCore* C = Cast<ATEAMBOXEnergyCore>(A);
			if (C && C->IsReceivingBeam()) NumEmittingCores++;
		}
		UE_LOG(LogTemp, Log, TEXT("[DeviceA Receive] %s 未激活，本帧未接收到光线。场景中已激活的 DeviceA 数量=%d，正在发射的 EnergyCore 数量=%d。若应有光线照到，请检查接收器碰撞是否 Block 射线通道(如 Visibility)。"),
			*GetName(), NumActiveDeviceA, NumEmittingCores);
	}

	if (bOneShotActivate)
	{
		if (bReceivingThisFrame)
		{
			bHasEverReceivedBeam = true;
			bIsActivated = true;
			if (ActivationSFX && !bWasActiveBeforeCheck)
			{
				UGameplayStatics::PlaySoundAtLocation(this, ActivationSFX, GetActorLocation());
			}
			UE_LOG(LogTemp, Log, TEXT("[DeviceA Receive] %s 接收到光线(来自 %s)，已激活(一次激活模式)。"), *GetName(), *ReceiveSourceDesc);
		}
	}
	else
	{
		const bool bWasActive = bIsActivated;
		bIsActivated = bReceivingThisFrame;
		if (bReceivingThisFrame && !bWasActive)
		{
			if (ActivationSFX)
			{
				UGameplayStatics::PlaySoundAtLocation(this, ActivationSFX, GetActorLocation());
			}
			UE_LOG(LogTemp, Log, TEXT("[DeviceA Receive] %s 接收到光线(来自 %s)，已激活(持续接收模式)。"), *GetName(), *ReceiveSourceDesc);
		}
		else if (!bReceivingThisFrame && bWasActive)
		{
			UE_LOG(LogTemp, Log, TEXT("[DeviceA Receive] %s 本帧未接收到光线，已关闭激活(持续接收模式)。"), *GetName());
		}
	}
}

void ATEAMBOXDeviceA::UpdateBeam()
{
	if (ArrowComponent == nullptr || BeamComponent == nullptr)
	{
		return;
	}

	// Get arrow location and direction
	FVector ArrowLocation = ArrowComponent->GetComponentLocation();
	FVector ArrowForward = ArrowComponent->GetForwardVector();
	FRotator ArrowRotation = ArrowComponent->GetComponentRotation();

	// DIAGNOSTIC: 仅在开启调试可视化时、每 60 帧打印一次，且带 Actor 名避免多实例时混在一起
	static int32 FrameCount = 0;
	const bool bShouldDiag = bEnableDebugVisualization && (FrameCount % 60 == 0);
	const FString DiagActorName = GetName();
	FrameCount++;
	if (bShouldDiag)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DeviceA DIAG][%s] Arrow Location: X=%.2f Y=%.2f Z=%.2f"), 
			*DiagActorName, ArrowLocation.X, ArrowLocation.Y, ArrowLocation.Z);
		UE_LOG(LogTemp, Warning, TEXT("[DeviceA DIAG][%s] Arrow Forward: X=%.4f Y=%.4f Z=%.4f"), 
			*DiagActorName, ArrowForward.X, ArrowForward.Y, ArrowForward.Z);
		UE_LOG(LogTemp, Warning, TEXT("[DeviceA DIAG][%s] Actor Location: X=%.2f Y=%.2f Z=%.2f"), 
			*DiagActorName, GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);
	}

	// Calculate end location (max range)
	FVector TraceEnd = ArrowLocation + (ArrowForward * MaxBeamRange);

	// Perform line trace
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = true;

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		ArrowLocation,
		TraceEnd,
		TraceChannel,
		QueryParams
	);

	// Update beam locations
	BeamStartLocation = ArrowLocation;
	
	if (bHit && HitResult.bBlockingHit)
	{
		BeamEndLocation = HitResult.Location;
		if (bShouldDiag)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DeviceA DIAG][%s] Hit! Location: X=%.2f Y=%.2f Z=%.2f Actor=%s"), 
				*DiagActorName, HitResult.Location.X, HitResult.Location.Y, HitResult.Location.Z,
				HitResult.GetActor() ? *HitResult.GetActor()->GetName() : TEXT("None"));
		}
	}
	else
	{
		BeamEndLocation = TraceEnd;
		if (bShouldDiag)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DeviceA DIAG][%s] No hit, using max range. Beam Start: X=%.2f Y=%.2f Z=%.2f End: X=%.2f Y=%.2f Z=%.2f"), 
				*DiagActorName, BeamStartLocation.X, BeamStartLocation.Y, BeamStartLocation.Z,
				BeamEndLocation.X, BeamEndLocation.Y, BeamEndLocation.Z);
		}
	}

	if (bShouldDiag)
	{
		FVector BeamDirection = (BeamEndLocation - BeamStartLocation).GetSafeNormal();
		float BeamLength = FVector::Dist(BeamStartLocation, BeamEndLocation);
		UE_LOG(LogTemp, Warning, TEXT("[DeviceA DIAG][%s] Beam Length=%.2f cm | BeamComponent at X=%.2f Y=%.2f Z=%.2f"), 
			*DiagActorName, BeamLength, BeamComponent->GetComponentLocation().X, BeamComponent->GetComponentLocation().Y, BeamComponent->GetComponentLocation().Z);
		UE_LOG(LogTemp, Warning, TEXT("[DeviceA DIAG][%s] Using WORLD SPACE coordinates"), *DiagActorName);
	}

	// Update Niagara Beam start and end points
	// IMPORTANT: You must create these User parameters in your Niagara Beam system first!
	// See detailed instructions below on how to set up the Niagara system.
	
	// Convert to local space if needed
	FVector BeamStartForNiagara = BeamStartLocation;
	FVector BeamEndForNiagara = BeamEndLocation;
	
	if (bUseLocalSpaceForBeam)
	{
		FTransform ComponentTransform = BeamComponent->GetComponentTransform();
		BeamStartForNiagara = ComponentTransform.InverseTransformPosition(BeamStartLocation);
		BeamEndForNiagara = ComponentTransform.InverseTransformPosition(BeamEndLocation);
		if (bShouldDiag)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DeviceA DIAG][%s] Using LOCAL SPACE | Beam Start(Local): X=%.2f Y=%.2f Z=%.2f"), 
				*DiagActorName, BeamStartForNiagara.X, BeamStartForNiagara.Y, BeamStartForNiagara.Z);
		}
	}
	
	// Method 1: Using SetNiagaraVariableVec3 (most reliable method)
	// This directly sets the variable in the Niagara system
	BeamComponent->SetNiagaraVariableVec3(FString("User.BeamStart"), BeamStartForNiagara);
	BeamComponent->SetNiagaraVariableVec3(FString("User.BeamEnd"), BeamEndForNiagara);
	
	// Method 2: Fallback - using SetVectorParameter
	BeamComponent->SetVectorParameter(FName("User.BeamStart"), BeamStartForNiagara);
	BeamComponent->SetVectorParameter(FName("User.BeamEnd"), BeamEndForNiagara);

	// Debug visualization (if enabled)
	if (bEnableDebugVisualization && World)
	{
		// Draw green line showing the calculated beam direction
		DrawDebugLine(World, BeamStartLocation, BeamEndLocation, FColor::Green, false, 0.0f, 0, 3.0f);
		
		// Draw red sphere at beam start
		DrawDebugSphere(World, BeamStartLocation, 10.0f, 12, FColor::Red, false, 0.0f, 0, 2.0f);
		
		// Draw blue sphere at beam end
		DrawDebugSphere(World, BeamEndLocation, 10.0f, 12, FColor::Blue, false, 0.0f, 0, 2.0f);
		
		// Draw yellow arrow showing arrow component forward direction
		DrawDebugDirectionalArrow(World, ArrowLocation, ArrowLocation + ArrowForward * 100.0f, 
			50.0f, FColor::Yellow, false, 0.0f, 0, 2.0f);
	}
}
