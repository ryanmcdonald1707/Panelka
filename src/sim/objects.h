// Kinds of usable things in the world. Shared by the generators (which place them)
// and the sim (which decides what you can do with them).
#pragma once

namespace sim {

enum class Obj : int {
    None = 0,
    // flat
    Bed, Wardrobe, Sofa, Table, Stove, Fridge, Cupboard, KitchenSink, Radio, TV,
    Toilet, BathSink, Bath, LightSwitch,
    // street
    BusStop, Kiosk,
};

const char* objName(Obj k);

} // namespace sim
