---
id: power/wp2-battery-charger
title: Battery charger
category: power
tags: [battery charger, recharger, charger, energy]
summary: A battery charger, recharger, or simply charger, is a device that stores energy in an electric battery by running current through it.
source: https://en.wikipedia.org/w/index.php?title=Battery_charger&oldid=1383601128
source_title: Wikipedia: Battery charger
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Battery charger", Wikipedia, The Free Encyclopedia (revision 1383601128), https://en.wikipedia.org/w/index.php?title=Battery_charger&oldid=1383601128 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Battery charger

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **battery charger**, **recharger**, or simply **charger**, is a device that stores energy in an electric battery by running current through it. The charging protocol—how much voltage and current, for how long and what to do when charging is complete—depends on the size and type of the battery being charged. Some battery types have high tolerance for overcharging after the battery has been fully charged and can be recharged by connection to a constant voltage source or a constant current source, depending on battery type.

Simple chargers of this type must be manually disconnected at the end of the charge cycle. Other battery types use a timer to cut off when charging should be complete. Other battery types cannot withstand over-charging, becoming damaged (reduced capacity, reduced lifetime), over heating or even exploding. The charger may have temperature or voltage sensing circuits and a microprocessor controller to safely adjust the charging current and voltage, determine the state of charge, and cut off at the end of charge. Chargers may elevate the output voltage proportionally with current to compensate for impedance in the wires.

A trickle charger provides a relatively small amount of current, only enough to counteract self-discharge of a battery that is idle for a long time. Some battery types cannot tolerate trickle charging; attempts to do so may result in damage. Lithium-ion batteries cannot handle indefinite trickle charging. Slow battery chargers may take several hours to complete a charge. High-rate chargers may restore most capacity much faster, but high-rate chargers can be more than some battery types can tolerate. Such batteries require active monitoring of the battery to protect it from any abusive use. Electric vehicles ideally need high-rate chargers. For public access, installation of such chargers and the distribution support for them is an issue in the proposed adoption of electric cars.

## C-rate

Charge and discharge rates are often given as *C* or *C-rate*, which is a measure of the rate at which a battery is charged or discharged relative to its capacity. The C-rate is defined as the charge or discharge current divided by the battery's capacity to store an electrical charge. While rarely stated explicitly, the unit of the C-rate is h⁻¹, equivalent to stating the battery's capacity to store an electrical charge in unit hour times current in the same unit as the charge or discharge current. The C-rate is never negative, so whether it describes a charging or discharging process depends on the context.

For example, for a battery with a capacity of 500 mAh, a discharge rate of 5000 mA (i.e., 5 A) corresponds to a C-rate of 10C, meaning that such a current can discharge 10 such batteries in one hour. Likewise, for the same battery a charge current of 250 mA corresponds to a C-rate of C/2, meaning that this current will increase the state of charge of this battery by 50% in one hour.

Running current through batteries generates internal heat, roughly proportional to the current involved (a battery's current state of charge, condition / history, etc. are also factors). If the charging process is endothermic (which is the case for Ni–Cd batteries, whereas charging nickel–metal hydride batteries is exothermic) the charging process initially cools the battery, but as it reaches full charge, the cooling effect stops and the cell heats up. Detecting a temperature rise of 10 C-change is one way of determining when to stop charging. Battery cells which have been built to allow higher C-rates than usual must make provision for increased heating.

But high C-ratings are attractive to end users because such batteries can be charged more quickly, and produce higher current output in use. High C-rates typically require the charger to carefully monitor battery parameters such as terminal voltage and temperature to prevent overcharging and so damage to the cells. Such high-charging rates are possible only with some battery types. Others will be damaged or possibly overheat or catch fire. Some batteries may even explode. For example, an automobile SLI (starting, lighting, ignition) lead–acid battery carries several risks of explosion.

## Type

### Simple charger

A simple charger works by supplying a constant DC or pulsed DC power source to a battery being charged. A simple charger typically does not alter its output based on charging time or the charge on the battery. This simplicity means that a simple charger is inexpensive, but there are tradeoffs. Typically, a carefully designed simple charger takes longer to charge a battery because it is set to use a lower (i.e., safer) charging rate. Even so, many batteries left on a simple charger for too long will be weakened or destroyed due to over-charging. These chargers also vary in that they can supply either a constant voltage or a constant current, to the battery.

Simple AC-powered battery chargers usually have much higher ripple current and ripple voltage than other kinds of battery chargers because they are inexpensively designed and built. Generally, when the ripple current is within a battery's manufacturer recommended level, the ripple voltage will also be well within the recommended level. The maximum ripple current for a typical 12 V 100 Ah VRLA battery is 5 amperes. As long as the ripple current is not excessive (more than 3 to 4 times the level recommended by the battery manufacturer), the expected life of a ripple-charged VRLA battery will be within 3% of the life of a constant DC-charged battery.

### Fast charger

Fast chargers make use of control circuitry to rapidly charge the batteries without damaging any of the cells in the battery. The control circuitry can be built into the battery (generally for each cell) or in the external charging unit, or split between both. Most such chargers have a cooling fan to help keep the temperature of the cells at safe levels. Most fast chargers are also capable of acting as standard overnight chargers if used with standard Ni–MH cells that do not have the special control circuitry.

### Three-stage charger

To accelerate the charging time and provide continuous charging, an intelligent charger attempts to detect the state of charge and condition of the battery and applies a three-stage charging scheme. The following description assumes a sealed lead–acid traction battery at 25 °C. The first stage is referred to as "bulk absorption"; the charging current is held high and constant and is limited by the capacity of the charger. When the voltage on the battery reaches its outgassing voltage (2.22 volts per cell) the charger switches to the second stage, and the voltage is held constant (2.40 volts per cell). The delivered current declines at the maintained voltage, and when the current reaches less than 0.005C the charger enters its third stage and the charger output is held constant at 2.25 volts per cell. In the third stage, the charging current is very small, 0.005C, and at this voltage the battery can be maintained at full charge and compensate for self-discharge.

### Induction-powered charger

Inductive battery chargers use electromagnetic induction to charge batteries. A charging station sends electromagnetic energy through inductive coupling to an electrical device, which stores the energy in the batteries. This is achieved without the need for metal contacts between the charger and the battery. Inductive battery chargers are commonly used in electric toothbrushes and other devices used in bathrooms. Because there are no open electrical contacts, there is no risk of electrocution. Nowadays it is being used to charge wireless phones.

### Smart charger

A smart charger can respond to the condition of a battery and modify its charging parameters accordingly, whereas "dumb" chargers apply a steady voltage, possibly through a fixed resistance. It should not be confused with a smart battery that contains a computer chip and communicates digitally with a smart charger about battery condition. A smart battery requires a smart charger. Some smart chargers can also charge "dumb" batteries, which lack any internal electronics.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Type, Applications, Prolonging battery life, Recent advances.)_
