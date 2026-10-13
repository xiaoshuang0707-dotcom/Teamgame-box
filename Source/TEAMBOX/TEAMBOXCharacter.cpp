// Copyright Epic Games, Inc. All Rights Reserved.

#include "TEAMBOXCharacter.h"
#include "TEAMBOXRuntimeEnergyCoreLimit.h"
#include "TEAMBOXProjectile.h"
#include "TEAMBOXEnergyCore.h"
#include "TEAMBOXSwitchLever.h"
#include "Animation/AnimInstance.h"
#include "Components/ArrowComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

void ATEAMBOXCharacter::RegisterOverlappingLever(ATEAMBOXSwitchLever* Lever)
{
	if (Lever && !OverlappingLevers.Contains(Lever))
	{
		OverlappingLevers.Add(Lever);
	}
}

void ATEAMBOXCharacter::UnregisterOverlappingLever(ATEAMBOXSwitchLever* Lever)
{
	if (Lever)
	{
		OverlappingLevers.Remove(Lever);
	}
}

void ATEAMBOXCharacter::TryInteract()
{
	if (OverlappingLevers.Num() > 0)
	{
		ATEAMBOXSwitchLever* Lever = OverlappingLevers[0];
		if (Lever)
		{
			Lever->ToggleSwitch();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// ATEAMBOXCharacter

ATEAMBOXCharacter::ATEAMBOXCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	// Initialize energy core defaults
	SummonDistance = 500.0f; // 5 meters in cm
	MaxEnergyCoreCount = 2;
	EnergyCoreClass = nullptr;
	HeldEnergyCore = nullptr;
	LastOperatedEnergyCore = nullptr;
	PickUpRange = 1000.0f; // 10 meters in cm
	EnergyCoreDistanceScrollSpeed = 50.0f; // 0.5 meters per scroll (50cm)
	SummonEnergyCoreSFX = nullptr;
}

void ATEAMBOXCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (TEAMBOXRuntimeEnergyCoreLimit::bHasPending)
	{
		MaxEnergyCoreCount = FMath::Clamp(TEAMBOXRuntimeEnergyCoreLimit::PendingMaxEnergyCoreCount, 1, 10);
		TEAMBOXRuntimeEnergyCoreLimit::bHasPending = false;
		TEAMBOXRuntimeEnergyCoreLimit::PendingMaxEnergyCoreCount = 0;
	}
}

//////////////////////////////////////////////////////////////////////////// Input

void ATEAMBOXCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ATEAMBOXCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATEAMBOXCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATEAMBOXCharacter::Look);

		// Summon Energy Core
		EnhancedInputComponent->BindAction(SummonEnergyCoreAction, ETriggerEvent::Started, this, &ATEAMBOXCharacter::SummonEnergyCore);

		// Pick Up / Drop Energy Core
		EnhancedInputComponent->BindAction(PickUpDropAction, ETriggerEvent::Started, this, &ATEAMBOXCharacter::PickUpOrDropEnergyCore);

		// Reset Energy Core Rotation
		EnhancedInputComponent->BindAction(ResetEnergyCoreRotationAction, ETriggerEvent::Started, this, &ATEAMBOXCharacter::ResetEnergyCoreRotation);

		// Rotate Energy Core Z axis (Q key)
		EnhancedInputComponent->BindAction(RotateEnergyCoreQAction, ETriggerEvent::Started, this, &ATEAMBOXCharacter::RotateEnergyCoreZ);

		// Rotate Energy Core Y axis (E key)
		EnhancedInputComponent->BindAction(RotateEnergyCoreEAction, ETriggerEvent::Started, this, &ATEAMBOXCharacter::RotateEnergyCoreY);

		// Adjust Energy Core Distance (Mouse Wheel)
		EnhancedInputComponent->BindAction(AdjustEnergyCoreDistanceAction, ETriggerEvent::Triggered, this, &ATEAMBOXCharacter::AdjustEnergyCoreDistance);

		// Interact (e.g. G key for lever switch)
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ATEAMBOXCharacter::TryInteract);
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void ATEAMBOXCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void ATEAMBOXCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATEAMBOXCharacter::SummonEnergyCore()
{
	// Check if we're already holding an energy core (either explicitly held or from summon list)
	bool bHasHeldCore = false;
	
	// Check explicitly held core
	if (HeldEnergyCore != nullptr && IsValid(HeldEnergyCore) && HeldEnergyCore->IsBeingHeld())
	{
		bHasHeldCore = true;
	}
	else
	{
		// Check if any summoned core is being held
		for (ATEAMBOXEnergyCore* Core : SpawnedEnergyCores)
		{
			if (IsValid(Core) && Core->IsBeingHeld())
			{
				bHasHeldCore = true;
				break;
			}
		}
	}
	
	// If we're holding a core, cannot summon a new one
	if (bHasHeldCore)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("Cannot summon energy core: already holding one"));
		return;
	}
	
	if (EnergyCoreClass == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("EnergyCoreClass is not set! Please assign it in the character blueprint."));
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (PlayerController == nullptr || FirstPersonCameraComponent == nullptr)
	{
		return;
	}

	// Get camera location and forward vector
	FVector CameraLocation = FirstPersonCameraComponent->GetComponentLocation();
	FVector CameraForwardVector = FirstPersonCameraComponent->GetForwardVector();

	// Calculate spawn location (5 meters in front of camera)
	FVector SpawnLocation = CameraLocation + (CameraForwardVector * SummonDistance);

	// Spawn facing uses camera yaw only (pitch/roll are fixed to zero).
	const FRotator CameraRotation = FirstPersonCameraComponent->GetComponentRotation();
	FRotator SpawnRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);

	// Check if we've reached the maximum count
	if (SpawnedEnergyCores.Num() >= MaxEnergyCoreCount)
	{
		// Remove the oldest energy core (FIFO)
		if (SpawnedEnergyCores.Num() > 0)
		{
			ATEAMBOXEnergyCore* OldestCore = SpawnedEnergyCores[0];
			if (IsValid(OldestCore))
			{
				OldestCore->Destroy();
			}
			SpawnedEnergyCores.RemoveAt(0);
		}
	}

	// Spawn collision handling
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	// Spawn the energy core
	ATEAMBOXEnergyCore* NewEnergyCore = World->SpawnActor<ATEAMBOXEnergyCore>(EnergyCoreClass, SpawnLocation, SpawnRotation, SpawnParams);
	
	if (NewEnergyCore != nullptr)
	{
		// Keep initial box facing fixed on summon and align beam forward to actor forward.
		NewEnergyCore->SetActorRotation(SpawnRotation);
		if (UArrowComponent* BeamStart = NewEnergyCore->GetBeamStartComponent())
		{
			BeamStart->SetWorldRotation(SpawnRotation);
		}

		if (SummonEnergyCoreSFX)
		{
			UGameplayStatics::PlaySoundAtLocation(this, SummonEnergyCoreSFX, SpawnLocation);
		}

		// Set target character for position updates
		NewEnergyCore->SetTargetCharacter(this);
		
		// Add to the list
		SpawnedEnergyCores.Add(NewEnergyCore);
		
		// Update last operated energy core
		LastOperatedEnergyCore = NewEnergyCore;
		
		UE_LOG(LogTemplateCharacter, Log, TEXT("Energy core summoned and set as last operated: %s"), *NewEnergyCore->GetName());
	}
}

void ATEAMBOXCharacter::PickUpOrDropEnergyCore()
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("PickUpOrDropEnergyCore called"));
	
	// Check if we're holding an energy core (either explicitly held or from summon list)
	ATEAMBOXEnergyCore* CoreToRelease = nullptr;
	
	// First check explicitly held core
	if (HeldEnergyCore != nullptr && IsValid(HeldEnergyCore))
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("Found explicitly held energy core"));
		CoreToRelease = HeldEnergyCore;
	}
	else
	{
		// Check if any summoned core is being held
		for (ATEAMBOXEnergyCore* Core : SpawnedEnergyCores)
		{
			if (IsValid(Core) && Core->IsBeingHeld())
			{
				UE_LOG(LogTemplateCharacter, Log, TEXT("Found held energy core in summon list"));
				CoreToRelease = Core;
				break;
			}
		}
	}
	
	// If we found a held core, release it
	if (CoreToRelease != nullptr)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("Releasing energy core: %s"), *CoreToRelease->GetName());
		CoreToRelease->Release();
		HeldEnergyCore = nullptr;
		
		// Clear last operated energy core if it's the one being released
		if (LastOperatedEnergyCore == CoreToRelease)
		{
			LastOperatedEnergyCore = nullptr;
		}
		
		return;
	}
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("No held energy core found, attempting to pick up"));

	// Otherwise, try to pick up an energy core
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("World is nullptr"));
		return;
	}
	
	if (FirstPersonCameraComponent == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("FirstPersonCameraComponent is nullptr"));
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (PlayerController == nullptr)
	{
		return;
	}

	// Get camera location and forward vector
	FVector CameraLocation = FirstPersonCameraComponent->GetComponentLocation();
	FVector CameraForwardVector = FirstPersonCameraComponent->GetForwardVector();

	// Calculate trace end location
	FVector TraceEnd = CameraLocation + (CameraForwardVector * PickUpRange);

	// Perform line trace to find energy cores
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = true;

	// Trace to find any blocking hit first
	ECollisionChannel TraceChannel = ECC_Visibility;
	
	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		CameraLocation,
		TraceEnd,
		TraceChannel,
		QueryParams
	);

	// Check if we hit an energy core
	if (bHit && HitResult.bBlockingHit)
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("Line trace hit something: %s"), *HitResult.GetActor()->GetName());
		
		ATEAMBOXEnergyCore* HitEnergyCore = Cast<ATEAMBOXEnergyCore>(HitResult.GetActor());
		if (HitEnergyCore != nullptr && IsValid(HitEnergyCore))
		{
			UE_LOG(LogTemplateCharacter, Log, TEXT("Hit energy core: %s, IsBeingHeld: %d"), *HitEnergyCore->GetName(), HitEnergyCore->IsBeingHeld());
			
			// Check if this energy core is already being held
			if (!HitEnergyCore->IsBeingHeld())
			{
				UE_LOG(LogTemplateCharacter, Log, TEXT("Picking up energy core: %s"), *HitEnergyCore->GetName());
				// Pick up this energy core
				HitEnergyCore->SetTargetCharacter(this);
				HitEnergyCore->Grab();
				HeldEnergyCore = HitEnergyCore;
				
				// Update last operated energy core
				LastOperatedEnergyCore = HitEnergyCore;
				
				UE_LOG(LogTemplateCharacter, Log, TEXT("Energy core picked up and set as last operated: %s"), *HitEnergyCore->GetName());
			}
			else
			{
				UE_LOG(LogTemplateCharacter, Log, TEXT("Energy core is already being held"));
			}
		}
		else
		{
			UE_LOG(LogTemplateCharacter, Log, TEXT("Hit actor is not an energy core"));
		}
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("Line trace did not hit anything"));
	}
}

void ATEAMBOXCharacter::ResetEnergyCoreRotation()
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("ResetEnergyCoreRotation called"));
	
	// Only reset the last operated energy core (last summoned or last picked up)
	if (LastOperatedEnergyCore != nullptr && IsValid(LastOperatedEnergyCore) && LastOperatedEnergyCore->IsBeingHeld())
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("Resetting rotation for last operated energy core: %s"), *LastOperatedEnergyCore->GetName());
		LastOperatedEnergyCore->ResetRotation();
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Log, TEXT("No valid last operated energy core to reset"));
	}
}

void ATEAMBOXCharacter::RotateEnergyCoreZ()
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("========== RotateEnergyCoreZ called (Q key) =========="));
	
	// Check LastOperatedEnergyCore
	if (LastOperatedEnergyCore == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("LastOperatedEnergyCore is nullptr"));
		return;
	}
	
	if (!IsValid(LastOperatedEnergyCore))
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("LastOperatedEnergyCore is not valid"));
		return;
	}
	
	if (!LastOperatedEnergyCore->IsBeingHeld())
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("LastOperatedEnergyCore is not being held: %s"), *LastOperatedEnergyCore->GetName());
		return;
	}
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("Calling RotateZ(90.0) on energy core: %s"), *LastOperatedEnergyCore->GetName());
	LastOperatedEnergyCore->RotateZ(90.0f);
	UE_LOG(LogTemplateCharacter, Log, TEXT("========== RotateEnergyCoreZ complete =========="));
}

void ATEAMBOXCharacter::RotateEnergyCoreY()
{
	UE_LOG(LogTemplateCharacter, Log, TEXT("========== RotateEnergyCoreY called (E key) =========="));
	
	// Check LastOperatedEnergyCore
	if (LastOperatedEnergyCore == nullptr)
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("LastOperatedEnergyCore is nullptr"));
		return;
	}
	
	if (!IsValid(LastOperatedEnergyCore))
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("LastOperatedEnergyCore is not valid"));
		return;
	}
	
	if (!LastOperatedEnergyCore->IsBeingHeld())
	{
		UE_LOG(LogTemplateCharacter, Warning, TEXT("LastOperatedEnergyCore is not being held: %s"), *LastOperatedEnergyCore->GetName());
		return;
	}
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("Calling RotateY(90.0) on energy core: %s"), *LastOperatedEnergyCore->GetName());
	LastOperatedEnergyCore->RotateY(90.0f);
	UE_LOG(LogTemplateCharacter, Log, TEXT("========== RotateEnergyCoreY complete =========="));
}

void ATEAMBOXCharacter::AdjustEnergyCoreDistance(const FInputActionValue& Value)
{
	// Only adjust distance if we have a valid last operated energy core that is being held
	if (LastOperatedEnergyCore == nullptr || !IsValid(LastOperatedEnergyCore))
	{
		return;
	}

	if (!LastOperatedEnergyCore->IsBeingHeld())
	{
		return;
	}

	// Get scroll value (positive = scroll up = increase distance, negative = scroll down = decrease distance)
	float ScrollValue = Value.Get<float>();
	
	// Calculate distance change (scroll speed in cm per scroll)
	float DistanceChange = ScrollValue * EnergyCoreDistanceScrollSpeed;
	
	// Get current target distance
	float CurrentTargetDistance = LastOperatedEnergyCore->GetTargetDistance();
	
	// Calculate new target distance
	float NewTargetDistance = CurrentTargetDistance + DistanceChange;
	
	// Set the new target distance (will be clamped inside SetTargetDistance)
	LastOperatedEnergyCore->SetTargetDistance(NewTargetDistance);
	
	UE_LOG(LogTemplateCharacter, Log, TEXT("AdjustEnergyCoreDistance: ScrollValue=%.2f, DistanceChange=%.2f cm, NewTargetDistance=%.2f cm"), 
		ScrollValue, DistanceChange, NewTargetDistance);
}