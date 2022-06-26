#include "FlightModule.hpp"
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/Entity.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/EntityTemplate.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/IComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/IPlacedComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/MeshComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/PhysicalMeshComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/PlaceholderComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/SlotComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/VisualControllerComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/ITransformBinding.hpp>
#include <RED4ext/Scripting/Natives/Generated/WorldWidgetComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/ent/HardTransformBinding.hpp>
#include <RED4ext/Scripting/Natives/Generated/world/ui/MeshTargetBinding.hpp>
#include <RED4ext/Scripting/Natives/Generated/physics/FilterData.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/projectile/SpawnComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/AttachmentSlots.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/AnimParamSlotsOption.hpp>
#include <RED4ext/Scripting/Natives/Generated/WidgetHudComponent.hpp>
#include <RED4ext/Scripting/Natives/Generated/vehicle/BaseObject.hpp>
#include <RED4ext/Scripting/Natives/Generated/vehicle/Weapon.hpp>
#include <RED4ext/Scripting/Natives/Generated/ink/HudEntriesResource.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/CameraComponent.hpp>
#include "LoadResRef.hpp"
#include <spdlog/spdlog.h>
#include "VehiclePhysicsUpdate.hpp"

// weird bug fix

// 48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 4C 89 64 24 20 55 41 56 41 57 48 8D 6C 24 C9 48 81
void __fastcall SpawnEffect(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
constexpr uintptr_t SpawnEffectAddr = 0x00007FF73F0D86B0 - 0x7ff73dfe0000;
decltype(&SpawnEffect) SpawnEffect_Original;

void __fastcall SpawnEffect(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6) {
  if (a1) {
    SpawnEffect_Original(a1, a2, a3, a4, a5, a6);
  }
}

RED4ext::ent::PhysicalMeshComponent *CreateThrusterEngine(RED4ext::CRTTISystem *rtti, RED4ext::CName mesh,
                                                          RED4ext::CName name, RED4ext::CName slot,
                                                          RED4ext::CName bindName = "vehicle_slots") {
  auto mc = (RED4ext::ent::PhysicalMeshComponent *)rtti->GetClass("entPhysicalMeshComponent")->AllocInstance();
  mc->mesh.ref = mesh;
  // not sure why this doesn't carry through
  //mc->isEnabled = false;
  mc->visualScale.X = 0.0;
  mc->visualScale.Y = 0.0;
  mc->visualScale.Z = 0.0;
  mc->filterDataSource = RED4ext::physics::FilterDataSource::Collider;
  auto filterData = (RED4ext::physics::FilterData *)rtti->GetClass("physicsFilterData")->AllocInstance();
  mc->filterData = RED4ext::Handle<RED4ext::physics::FilterData>(filterData);
  mc->name = name;
  mc->motionBlurScale = 0.1;
  mc->meshAppearance = "default";
  mc->objectTypeID = RED4ext::ERenderObjectType::ROT_Vehicle;
  mc->LODMode = RED4ext::ent::MeshComponentLODMode::Appearance;
  auto htb = (RED4ext::ent::HardTransformBinding *)rtti->GetClass("entHardTransformBinding")->AllocInstance();
  htb->bindName = bindName;
  htb->slotName = slot;
  mc->parentTransform = RED4ext::Handle<RED4ext::ent::ITransformBinding>(htb);
  return mc;
}

void AddToController(RED4ext::CRTTISystem *rtti, RED4ext::ent::VisualControllerComponent *vcc,
                     RED4ext::ent::MeshComponent *mc) {
  auto vcd = reinterpret_cast<RED4ext::ent::VisualControllerDependency *>(
      rtti->GetClass("entVisualControllerDependency")->AllocInstance());
  vcd->appearanceName = mc->meshAppearance;
  vcd->componentName = mc->name;
  vcd->mesh.ref = mc->mesh.ref;
  vcc->appearanceDependency.EmplaceBack(*vcd);
}

// 48 8B C4 55 56 41 54 41 55 41 56 41 57 48 8D A8 28 FE FF FF 48 81 EC A8 02 00 00 44 0F 29 A8 48
void __fastcall VehicleProcessWeapons(RED4ext::vehicle::BaseObject *vehicle, float timeDelta, unsigned int shootIndex);
constexpr uintptr_t VehicleProcessWeaponsAddr = 0x140000C00 + 0x1C5F1D0 - RED4ext::Addresses::ImageBase;
decltype(&VehicleProcessWeapons) VehicleProcessWeapons_Original;


void __fastcall VehicleProcessWeapons(RED4ext::vehicle::BaseObject *vehicle, float timeDelta, unsigned int shootIndex) {
  VehicleProcessWeapons_Original(vehicle, timeDelta, shootIndex);
  if (vehicle->weapons[shootIndex].cycleTimer == 0.0) {
    RED4ext::Quaternion quat = {0.0, 0.0, 0.0, 1.0};
    auto ph = vehicle->weapons[shootIndex].weaponObject.GetPtr()->placeholder;
    if (ph) {
      quat = ph->worldTransform.Orientation;
    }

    auto rtti = RED4ext::CRTTISystem::Get();
    auto fcc = rtti->GetClass("FlightComponent");
    auto fc = GetFlightComponent(vehicle);
    auto onFireWeapon = fcc->GetFunction("OnFireWeapon");
    auto args = RED4ext::CStackType(rtti->GetType("Quaternion"), &quat);
    auto stack = RED4ext::CStack(fc, &args, 1, nullptr, 0);
    onFireWeapon->Execute(&stack);
  }
}

// 48 89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 B8 FB FF FF 48 81 EC 48 05 00 00
void __fastcall Entity_InitializeComponents(RED4ext::ent::Entity *entity, uintptr_t, uintptr_t);
constexpr uintptr_t Entity_InitializeComponentsAddr = 0x140000C00 + 0x1035B00 - RED4ext::Addresses::ImageBase;
decltype(&Entity_InitializeComponents) Entity_InitializeComponents_Original;


void __fastcall Entity_InitializeComponents(RED4ext::ent::Entity* entity, uintptr_t a2, uintptr_t a3) {

  auto rtti = RED4ext::CRTTISystem::Get();
  
  auto type = entity->GetNativeType();
  auto isVehicle = false;
  do {
    isVehicle |= type == rtti->GetClass("vehicleCarBaseObject");
  } while (type = type->parent);

  if (isVehicle) {
    auto vehicle = reinterpret_cast<RED4ext::vehicle::BaseObject *>(entity);

   {
      auto weapon = new RED4ext::vehicle::Weapon();
      weapon->attackRange = 100.0;
      weapon->canFriendlyFire = true;
      weapon->cycleTime = 0.5;
      weapon->genericShoot = true;
      weapon->genericTick = true;
      weapon->item = RED4ext::TweakDBID("Items.Panzer_Cannon");
      weapon->maxPitch = 90.0;
      weapon->maxYaw = 120.0;
      weapon->minPitch = -90.0;
      weapon->minYaw = -120.0;
      weapon->singleProjectileCycleTime = 1.0;
      weapon->singleShotProjectiles = 1;
      weapon->slot = RED4ext::TweakDBID("AttachmentSlots.PanzerCannon");
      weapon->weaponShootAnimEvent = "shoot_rocket";
      weapon->wholeBurstProjectiles = 1;
      vehicle->weapons.EmplaceBack(*weapon);
   }
   //{
   //  auto weapon = new RED4ext::vehicle::Weapon();
   //  weapon->attackRange = 100.0;
   //  weapon->canFriendlyFire = true;
   //  weapon->cycleTime = 1.0;
   //  weapon->genericShoot = true;
   //  weapon->genericTick = true;
   //  weapon->item = RED4ext::TweakDBID("Items.Griffin_Rifle_Left");
   //  weapon->maxPitch = 90.0;
   //  weapon->maxYaw = 180.0;
   //  weapon->minPitch = -90.0;
   //  weapon->minYaw = -180.0;
   //  weapon->singleProjectileCycleTime = 1.0;
   //  weapon->singleShotProjectiles = 1;
   //  weapon->slot = RED4ext::TweakDBID("AttachmentSlots.WeaponLeft");
   //  weapon->weaponShootAnimEvent = "shoot_rocket";
   //  weapon->wholeBurstProjectiles = 1;
   //  vehicle->weapons.EmplaceBack(*weapon);
   //}
   //{
   //  auto weapon = new RED4ext::vehicle::Weapon();
   //  weapon->attackRange = 100.0;
   //  weapon->canFriendlyFire = true;
   //  weapon->cycleTime = 0.1;
   //  weapon->genericShoot = true;
   //  weapon->genericTick = true;
   //  weapon->item = RED4ext::TweakDBID("Items.CPO_HMG");
   //  weapon->maxPitch = 90.0;
   //  weapon->maxYaw = 180.0;
   //  weapon->minPitch = -90.0;
   //  weapon->minYaw = -180.0;
   //  weapon->singleProjectileCycleTime = 1.0;
   //  weapon->singleShotProjectiles = 1;
   //  weapon->slot = RED4ext::TweakDBID("AttachmentSlots.WeaponRight");
   //  weapon->weaponShootAnimEvent = "shoot_rocket";
   //  weapon->wholeBurstProjectiles = 1;
   //  vehicle->weapons.EmplaceBack(*weapon);
   //}

    // entVisualControllerComponent
    RED4ext::ent::VisualControllerComponent *vcc = NULL;
    for (auto const &handle : entity->components) {
      auto component = handle.GetPtr();
      if (component->GetNativeType() == rtti->GetClass("entVisualControllerComponent")) {
        vcc = reinterpret_cast<RED4ext::ent::VisualControllerComponent *>(component);
        break;
      }
    }

    for (auto const &handle : entity->components) {
      auto component = handle.GetPtr();
      if (component->GetNativeType() == rtti->GetClass("entSlotComponent")) {
        auto sc = reinterpret_cast<RED4ext::ent::SlotComponent *>(component);

        if (sc->name == "vehicle_slots") {
          {
            auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
            slot->boneName = "swingarm_front_left";
            slot->relativePosition.X -= 0.25;
            slot->relativePosition.Y += 0.75;
            slot->slotName = "PanzerCannon";
            sc->slots.EmplaceBack(*slot);
            sc->slotIndexLookup.Emplace(slot->slotName, sc->slots.size - 1);
          }
          {
            auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
            slot->boneName = "swingarm_front_left";
            slot->relativePosition.X -= 0.25;
            //slot->relativePosition.Y += 0.75;
            slot->slotName = "WeaponLeft";
            sc->slots.EmplaceBack(*slot);
            sc->slotIndexLookup.Emplace(slot->slotName, sc->slots.size - 1);
          }
          {
            auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
            slot->boneName = "swingarm_front_right";
            slot->relativePosition.X += 0.25;
            //slot->relativePosition.Y += 0.75;
            slot->slotName = "WeaponRight";
            sc->slots.EmplaceBack(*slot);
            sc->slotIndexLookup.Emplace(slot->slotName, sc->slots.size - 1);
          }
          {
            auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
            slot->boneName = "swingarm_front_left";
            slot->relativePosition.X = -0.25;
            slot->relativePosition.Z = -0.6;
            slot->relativeRotation.k = 1.0;
            slot->relativeRotation.r = 0.0;
            slot->slotName = "gun_front_left";
            sc->slots.EmplaceBack(*slot);
            sc->slotIndexLookup.Emplace(slot->slotName, sc->slots.size - 1);
          }
          {
            auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
            slot->boneName = "swingarm_front_left";
            slot->slotName = "thruster_front_left";
            sc->slots.EmplaceBack(*slot);
            sc->slotIndexLookup.Emplace(slot->slotName, sc->slots.size - 1);
          }
          {
            auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
            slot->boneName = "swingarm_front_right";
            slot->slotName = "thruster_front_right";
            sc->slots.EmplaceBack(*slot);
            sc->slotIndexLookup.Emplace(slot->slotName, sc->slots.size - 1);
          }
          {
            auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
            slot->boneName = "swingarm_back_left";
            slot->slotName = "thruster_back_left";
            sc->slots.EmplaceBack(*slot);
            sc->slotIndexLookup.Emplace(slot->slotName, sc->slots.size - 1);
          }
          {
            auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
            slot->boneName = "swingarm_back_right";
            slot->slotName = "thruster_back_right";
            sc->slots.EmplaceBack(*slot);
            sc->slotIndexLookup.Emplace(slot->slotName, sc->slots.size - 1);
          }
        }

        break;
      }
    }

    if (vcc != NULL) {
      {
        // SlotComponent
        //auto sc = (RED4ext::ent::SlotComponent *)rtti->GetClass("entSlotComponent")->AllocInstance();
        //sc->name = "thruster_slots";
        //auto htb = (RED4ext::ent::HardTransformBinding *)rtti->GetClass("entHardTransformBinding")->AllocInstance();
        //htb->bindName = "deformation_rig";
        //sc->parentTransform = RED4ext::Handle<RED4ext::ent::ITransformBinding>(htb);

        //{
        //  auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
        //  slot->boneName = "swingarm_front_left";
        //  slot->slotName = "thruster_front_left";
        //  sc->slots.EmplaceBack(*slot);
        //  sc->slotIndexLookup.Emplace(slot->slotName, 0);
        //}
        //{
        //  auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
        //  slot->boneName = "swingarm_front_right";
        //  slot->slotName = "thruster_front_right";
        //  sc->slots.EmplaceBack(*slot);
        //  sc->slotIndexLookup.Emplace(slot->slotName, 1);
        //}
        //{
        //  auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
        //  slot->boneName = "swingarm_back_left";
        //  slot->slotName = "thruster_back_left";
        //  sc->slots.EmplaceBack(*slot);
        //  sc->slotIndexLookup.Emplace(slot->slotName, 2);
        //}
        //{
        //  auto slot = reinterpret_cast<RED4ext::ent::Slot *>(rtti->GetClass("entSlot")->AllocInstance());
        //  slot->boneName = "swingarm_back_right";
        //  slot->slotName = "thruster_back_right";
        //  sc->slots.EmplaceBack(*slot);
        //  sc->slotIndexLookup.Emplace(slot->slotName, 3);
        //}

        //entity->components.EmplaceBack(RED4ext::Handle<RED4ext::ent::SlotComponent>(sc));
      }

      {
        auto fl = CreateThrusterEngine(rtti, "user\\jackhumbert\\meshes\\engine_corpo.mesh", "ThrusterFL",
                                       "thruster_front_left");
        entity->components.EmplaceBack(RED4ext::Handle<RED4ext::ent::PhysicalMeshComponent>(fl));
        AddToController(rtti, vcc, fl);


        //auto mc = (RED4ext::ent::MeshComponent *)rtti->GetClass("entMeshComponent")->AllocInstance();
        //mc->mesh.ref =
        //    (RED4ext::CName)"base\\weapons\\turrets\\maxtac_turret\\entities\\meshes\\w_turret__maxtac_turret__base1_guns.mesh";
        //mc->visualScale.X = 1.0;
        //mc->visualScale.Y = 1.0;
        //mc->visualScale.Z = 1.0;
        //mc->name = "FunGun";
        //mc->motionBlurScale = 0.1;
        //mc->meshAppearance = "default";
        //mc->objectTypeID = RED4ext::ERenderObjectType::ROT_Vehicle;
        //mc->LODMode = RED4ext::ent::MeshComponentLODMode::Appearance;
        //auto htb = (RED4ext::ent::HardTransformBinding *)rtti->GetClass("entHardTransformBinding")->AllocInstance();
        //htb->bindName = "vehicle_slots";
        //htb->slotName = "gun_front_left";
        //mc->parentTransform = RED4ext::Handle<RED4ext::ent::ITransformBinding>(htb);
        //entity->components.EmplaceBack(RED4ext::Handle<RED4ext::ent::MeshComponent>(mc));
        //AddToController(rtti, vcc, mc);

        auto fr = CreateThrusterEngine(rtti, "user\\jackhumbert\\meshes\\engine_corpo.mesh", "ThrusterFR",
                                       "thruster_front_right");
        entity->components.EmplaceBack(RED4ext::Handle<RED4ext::ent::PhysicalMeshComponent>(fr));
        AddToController(rtti, vcc, fr);

        auto bl = CreateThrusterEngine(rtti, "user\\jackhumbert\\meshes\\engine_corpo.mesh", "ThrusterBL",
                                       "thruster_back_left");
        entity->components.EmplaceBack(RED4ext::Handle<RED4ext::ent::PhysicalMeshComponent>(bl));
        AddToController(rtti, vcc, bl);

        auto br = CreateThrusterEngine(rtti, "user\\jackhumbert\\meshes\\engine_corpo.mesh", "ThrusterBR",
                                       "thruster_back_right");
        entity->components.EmplaceBack(RED4ext::Handle<RED4ext::ent::PhysicalMeshComponent>(br));
        AddToController(rtti, vcc, br);
      }
    }

    // UI
    //{
    //  // MeshComponent
    //  auto mc = (RED4ext::ent::MeshComponent *)rtti->GetClass("entMeshComponent")->AllocInstance();

    //  RED4ext::CName mesh = "user\\jackhumbert\\meshes\\flight_ui.mesh";
    //  mc->mesh.ref = mesh;
    //  mc->name = "flight_screen";
    //  mc->meshAppearance = "default";

    //  auto mcHandle = RED4ext::Handle<RED4ext::ent::MeshComponent>(mc);
    //  entity->components.EmplaceBack(mcHandle);
    //}
    //{
    //  // WorldWidgetComponent
    //  auto wwc = (RED4ext::WorldWidgetComponent *)rtti->GetClass("WorldWidgetComponent")->AllocInstance();

    //  wwc->name = "flight_ui";

    //  RED4ext::CName fc = "user\\jackhumbert\\widgets\\flight_ui.inkwidget";
    //  wwc->widgetResource.ref = fc;

    //  auto mtb = (RED4ext::world::ui::MeshTargetBinding
    //  *)rtti->GetClass("worlduiMeshTargetBinding")->AllocInstance(); mtb->bindName = "flight_screen";
    //  wwc->meshTargetBinding = RED4ext::Handle<RED4ext::world::ui::MeshTargetBinding>(mtb);

    //  auto wwcHandle = RED4ext::Handle<RED4ext::WorldWidgetComponent>(wwc);
    //  entity->components.EmplaceBack(wwcHandle);
    //}

    // UI Info Panel
    {
      // MeshComponent
      auto mc = (RED4ext::ent::MeshComponent *)rtti->GetClass("entMeshComponent")->AllocInstance();

      RED4ext::CName mesh = "user\\jackhumbert\\meshes\\flight_ui_info.mesh";
      mc->mesh.ref = mesh;
      mc->name = "flight_screen_info";
      mc->meshAppearance = "screen_ui";
      mc->renderingPlane = RED4ext::ERenderingPlane::RPl_Weapon;
      mc->forcedLodDistance = RED4ext::ent::ForcedLodDistance::VehicleInterior;

      entity->components.EmplaceBack(RED4ext::Handle<RED4ext::ent::MeshComponent>(mc));
    }
    {
      // WorldWidgetComponent
      auto wwc = (RED4ext::WorldWidgetComponent *)rtti->GetClass("WorldWidgetComponent")->AllocInstance();

      wwc->name = "flight_ui_info";

      RED4ext::CName fc = "user\\jackhumbert\\widgets\\flight_ui.inkwidget";
      wwc->widgetResource.ref = fc;
      wwc->spawnDistanceOverride = 20.0;
      wwc->sceneWidgetProperties.renderingPlane = RED4ext::ERenderingPlane::RPl_Weapon;
      auto mtb = (RED4ext::world::ui::MeshTargetBinding *)rtti->GetClass("worlduiMeshTargetBinding")->AllocInstance();
      mtb->bindName = "flight_screen_info";
      wwc->meshTargetBinding = RED4ext::Handle<RED4ext::world::ui::MeshTargetBinding>(mtb);

      entity->components.EmplaceBack(RED4ext::Handle<RED4ext::WorldWidgetComponent>(wwc));
    }
    { 
      auto gpsp = (RED4ext::game::projectile::SpawnComponent *)rtti->GetClass("gameprojectileSpawnComponent")->AllocInstance();
      gpsp->name = "projectileSpawn8722";
      gpsp->projectileTemplates.EmplaceBack("exploding_bullet");
      auto htb = (RED4ext::ent::HardTransformBinding *)rtti->GetClass("entHardTransformBinding")->AllocInstance();
      htb->bindName = "vehicle_slots";
      htb->slotName = "ThrusterFL";
      gpsp->parentTransform = RED4ext::Handle<RED4ext::ent::ITransformBinding>(htb);
      entity->components.EmplaceBack(RED4ext::Handle<RED4ext::game::projectile::SpawnComponent>(gpsp));
    }
    {
      auto gas = (RED4ext::game::AttachmentSlots *)rtti->GetClass("gameAttachmentSlots")->AllocInstance();
      gas->name = "AttachmentSlots";
      {
        auto gapso = (RED4ext::game::AnimParamSlotsOption *)rtti->GetClass("gameAnimParamSlotsOption")->AllocInstance();
        gapso->slotID = "AttachmentSlots.PanzerCannon";
        gapso->paramName = "renderPlane";
        gas->animParams.EmplaceBack(*gapso);
      }
      {
        auto gapso = (RED4ext::game::AnimParamSlotsOption *)rtti->GetClass("gameAnimParamSlotsOption")->AllocInstance();
        gapso->slotID = "AttachmentSlots.WeaponLeft";
        gapso->paramName = "renderPlane";
        gas->animParams.EmplaceBack(*gapso);
      }
      {
        auto gapso = (RED4ext::game::AnimParamSlotsOption *)rtti->GetClass("gameAnimParamSlotsOption")->AllocInstance();
        gapso->slotID = "AttachmentSlots.WeaponRight";
        gapso->paramName = "renderPlane";
        gas->animParams.EmplaceBack(*gapso);
      }
      entity->components.EmplaceBack(RED4ext::Handle<RED4ext::game::AttachmentSlots>(gas));
    }

    {
      auto whc = (RED4ext::WidgetHudComponent *)rtti->GetClass("WidgetHudComponent")->AllocInstance();
      whc->name = "FlightHUD";
      whc->hudEntriesResource.hash = (RED4ext::CName)"user\\jackhumbert\\widgets\\hud_flight.inkhud";
      LoadResRef<RED4ext::ink::HudEntriesResource>(&whc->hudEntriesResource.hash, &whc->hudEntriesResource.handle, false);
      entity->components.EmplaceBack(RED4ext::Handle<RED4ext::WidgetHudComponent>(whc));
    }

    { 
      auto gcc = (RED4ext::game::CameraComponent *)rtti->GetClass("gameCameraComponent")->AllocInstance();
      gcc->name = "frontCamera";
      auto htb = (RED4ext::ent::HardTransformBinding *)rtti->GetClass("entHardTransformBinding")->AllocInstance();
      htb->bindName = "vehicle_slots";
      htb->slotName = "Base";
      gcc->parentTransform = RED4ext::Handle<RED4ext::ent::ITransformBinding>(htb);
      entity->components.EmplaceBack(RED4ext::Handle<RED4ext::game::CameraComponent>(gcc));
    }

  }


  Entity_InitializeComponents_Original(entity, a2, a3);
}

void EntityAddComponent(RED4ext::IScriptable *aContext, RED4ext::CStackFrame *aFrame, bool *aOut, int64_t a4) {
  auto ent = reinterpret_cast<RED4ext::ent::Entity *>(aContext);

  RED4ext::Handle<RED4ext::ent::IComponent> value;
  RED4ext::GetParameter(aFrame, &value);
  aFrame->code++; // skip ParamEnd
  // auto handle = RED4ext::Handle<RED4ext::ent::IComponent>(value);
  // auto handle = *(RED4ext::Handle<RED4ext::ent::IComponent> *)&value;
  // ent->components.EmplaceBack(handle);
  ent->components.EmplaceBack(value);

  if (aOut) {
    *aOut = true;
  }
}

void EntityAddWorldWidgetComponent(RED4ext::IScriptable *aContext, RED4ext::CStackFrame *aFrame, bool *aOut, int64_t a4) {
  auto ent = reinterpret_cast<RED4ext::ent::Entity *>(aContext);

  aFrame->code++; // skip ParamEnd

  auto rtti = RED4ext::CRTTISystem::Get();


  // MeshComponent
  auto mc = (RED4ext::ent::MeshComponent*)rtti->GetClass("entMeshComponent")->AllocInstance();
  
  RED4ext::CName mesh =
      "base\\environment\\decoration\\advertising\\holograms\\common\\common_holograms_transparent_a_w500_h150.mesh";
  mc->mesh.ref = mesh;
  mc->name = "radio_screen";
  mc->meshAppearance = "screen_ui";
  mc->localTransform.Position.x.Bits = 216270;
  mc->visualScale.X = 10.0;
  mc->visualScale.X = 10.0;
  mc->visualScale.X = 10.0;

  auto mcHandle = RED4ext::Handle<RED4ext::ent::MeshComponent>(mc);
  ent->components.EmplaceBack(mcHandle);

  // WorldWidgetComponent
  auto wwc = (RED4ext::WorldWidgetComponent *)rtti->GetClass("WorldWidgetComponent")->AllocInstance();

  wwc->name = "radio_ui";

  RED4ext::CName fc = "base\\gameplay\\gui\\world\\radio\\radio_ui.inkwidget";
  wwc->widgetResource.ref = fc;

  auto mtb = (RED4ext::world::ui::MeshTargetBinding *)rtti->GetClass("worlduiMeshTargetBinding")->AllocInstance();
  mtb->bindName = "radio_screen";
  wwc->meshTargetBinding = RED4ext::Handle<RED4ext::world::ui::MeshTargetBinding>(mtb);

  auto wwcHandle = RED4ext::Handle<RED4ext::WorldWidgetComponent>(wwc);
  ent->components.EmplaceBack(wwcHandle);

  if (aOut) {
    *aOut = true;
  }
}

//base\vehicles\common\gameplay\flight_components.ent

//void EntityAddFlightComponent(RED4ext::IScriptable *aContext, RED4ext::CStackFrame *aFrame, bool *aOut, int64_t a4) {
//  auto ent = reinterpret_cast<RED4ext::ent::Entity *>(aContext);
//
//
//  RED4ext::CName *name;
//  RED4ext::GetParameter(aFrame, &name);
//  aFrame->code++; // skip ParamEnd
//
//  if (name != 0) {
//    auto resHandle = new RED4ext::ResourceHandle<RED4ext::ent::EntityTemplate>();
//    RED4ext::CName fc = "base\\vehicles\\common\\gameplay\\flight_components.ent";
//    LoadResRef<RED4ext::ent::EntityTemplate>((uint64_t *)&fc, resHandle, true);
//
//    auto et = resHandle->wrapper->resource.GetPtr();
//    et->sub_28(true);
//
//    auto handle = RED4ext::Handle<RED4ext::ent::IComponent>(value);
//    ent->components.EmplaceBack(handle);
//
//    if (aOut) {
//      *aOut = true;
//    }
//  }
//}

void IPlacedComponentUpdateHardTransformBinding(RED4ext::IScriptable *aContext, RED4ext::CStackFrame *aFrame, bool *aOut, int64_t a4) {
  auto ipc = reinterpret_cast<RED4ext::ent::IPlacedComponent *>(aContext);

  RED4ext::CName bindName;
  RED4ext::CName slotName;
  RED4ext::GetParameter(aFrame, &bindName);
  RED4ext::GetParameter(aFrame, &slotName);
  aFrame->code++; // skip ParamEnd

  auto rtti = RED4ext::CRTTISystem::Get();
  auto transform = ipc->parentTransform.GetPtr();
  if (transform->GetNativeType() == rtti->GetClass("entHardTransformBinding")) {
    auto ht = (RED4ext::ent::HardTransformBinding *)(transform);
    ht->bindName = bindName;
    ht->slotName = slotName;
    if (aOut) {
      *aOut = true;
    }
  } else {
    if (aOut) {
      *aOut = false;
    }
  }

}

struct EntityAddComponentModule : FlightModule {
  void Load(const RED4ext::Sdk *aSdk, RED4ext::PluginHandle aHandle) {
    aSdk->hooking->Attach(aHandle, RED4EXT_OFFSET_TO_ADDR(Entity_InitializeComponentsAddr),
                          &Entity_InitializeComponents,
                          reinterpret_cast<void **>(&Entity_InitializeComponents_Original));
    aSdk->hooking->Attach(aHandle, RED4EXT_OFFSET_TO_ADDR(VehicleProcessWeaponsAddr), &VehicleProcessWeapons,
                          reinterpret_cast<void **>(&VehicleProcessWeapons_Original));
    aSdk->hooking->Attach(aHandle, RED4EXT_OFFSET_TO_ADDR(SpawnEffectAddr), &SpawnEffect,
                          reinterpret_cast<void **>(&SpawnEffect_Original));
  }
  void Unload(const RED4ext::Sdk *aSdk, RED4ext::PluginHandle aHandle) {
    aSdk->hooking->Detach(aHandle, RED4EXT_OFFSET_TO_ADDR(Entity_InitializeComponentsAddr));
    aSdk->hooking->Detach(aHandle, RED4EXT_OFFSET_TO_ADDR(VehicleProcessWeaponsAddr));
    aSdk->hooking->Detach(aHandle, RED4EXT_OFFSET_TO_ADDR(SpawnEffectAddr));
  }
  void PostRegisterTypes() {
    auto rtti = RED4ext::CRTTISystem::Get();
    auto ent = rtti->GetClass("entEntity");
    ent->RegisterFunction(
        RED4ext::CClassFunction::Create(ent, "AddComponent", "AddComponent", &EntityAddComponent, {.isNative = true}));
    ent->RegisterFunction(
        RED4ext::CClassFunction::Create(ent, "AddWorldWidgetComponent", "AddWorldWidgetComponent", &EntityAddWorldWidgetComponent, {.isNative = true}));

    auto ipc = rtti->GetClass("entIPlacedComponent");
    ipc->RegisterFunction(RED4ext::CClassFunction::Create(ipc, "UpdateHardTransformBinding", "UpdateHardTransformBinding",
                                                          &IPlacedComponentUpdateHardTransformBinding, {.isNative = true}));

    
    auto mc = rtti->GetClass("entMeshComponent");
    mc->props.EmplaceBack(RED4ext::CProperty::Create(rtti->GetType("Vector3"), "visualScale", nullptr, 0x178));

  }
};

REGISTER_FLIGHT_MODULE(EntityAddComponentModule);