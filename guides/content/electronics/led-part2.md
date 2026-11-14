---
id: electronics/led-part2
title: Light-emitting diode (part 2 of 3)
category: electronics
tags: [electronics, components, LED, lighting]
summary: "Light-emitting diodes: how LEDs work, how to drive them and their use in efficient lighting. (continued, part 2)"
source: "https://en.wikipedia.org/w/index.php?title=Light-emitting_diode&oldid=1385505056"
source_title: Light-emitting diode (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Light-emitting diode' (rev 1385505056), Wikipedia, https://en.wikipedia.org/wiki/Light-emitting_diode. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
part_of: electronics/led
order: 2
---

# Light-emitting diode (part 2 of 3)

## Considerations for use

- Efficiency: LEDs emit more lumens per watt than incandescent light bulbs. The efficiency of LED lighting fixtures is not affected by shape and size, unlike fluorescent light bulbs or tubes.
- Size: LEDs can be very small (smaller than 2 mm^2) and are easily attached to printed circuit boards.

### Power sources

The current in an LED or other diodes rises exponentially with the applied voltage (see Shockley diode equation), so a small change in voltage can cause a large change in current. Current through the LED must be regulated by an external circuit such as a constant current source to prevent damage.

LEDs are sensitive to voltage. They must be supplied with a voltage above their threshold voltage and a current below their rating. Current and lifetime change greatly with a small change in applied voltage. They thus require a current-regulated supply (usually just a series resistor for indicator LEDs).

Efficiency droop: The efficiency of LEDs decreases as the electric current increases. Heating also increases with higher currents, which compromises LED lifetime. These effects put practical limits on the current through an LED in high-power applications.

### Electrical polarity

Unlike a traditional incandescent lamp, an LED will light only when voltage is applied in the forward direction of the diode. No current flows and no light is emitted if voltage is applied in the reverse direction. If the reverse voltage exceeds the breakdown voltage, which is typically about five volts, a large current flows and the LED will be damaged.

By definition, the energy band gap of any diode is higher when reverse-biased than when forward-biased. Because the band gap energy determines the wavelength of the light emitted, the color cannot be the same when reverse-biased. The reverse breakdown voltage is sufficiently high that the emitted wavelength cannot be similar enough to still be visible. Though dual-LED packages exist that contain a different color LED in each direction, it is not expected that any single LED element can emit visible light when reverse-biased.

It is not known if any Zener diode could exist that emits light only in reverse-bias mode. Uniquely, this type of LED would conduct when connected backwards.

### Appearance

- Color: LEDs can emit light of an intended color without using any color filters, as traditional lighting methods need. This is more efficient and can lower initial costs.
- Cool light: In contrast to most light sources, LEDs radiate very little heat in the form of IR that can cause damage to sensitive objects or fabrics. Wasted energy is dispersed as heat through the base of the LED.
- Color rendition: Most cool-white LEDs have spectra that differ significantly from a black body radiator like the sun or an incandescent light. The spike at 460 nm and dip at 500 nm can make the color of objects appear differently under cool-white LED illumination than sunlight or incandescent sources, due to metamerism, red surfaces being rendered particularly poorly by typical phosphor-based cool-white LEDs. The same is true with green surfaces. The quality of color rendition of an LED is measured by the Color Rendering Index (CRI).
- Dimming: LEDs can be dimmed either by pulse-width modulation or lowering the forward current. This pulse-width modulation is why LED lights, particularly headlights on cars, when viewed on camera or by some people, seem to flash or flicker. This is a type of stroboscopic effect.

### Light properties

- Switch-on time: LEDs light up extremely quickly. A typical red indicator LED achieves full brightness in under a microsecond. LEDs used in communications devices can have even faster response times.
- Focus: The solid package of the LED can be designed to focus its light. Incandescent and fluorescent sources often require an external reflector to collect light and direct it in a usable manner. For larger LED packages, total internal reflection (TIR) lenses are often used to the same effect. When large quantities of light are needed, many light sources such as LED chips are usually deployed, which are difficult to focus or collimate on the same target.
- Area light source: Single LEDs do not approximate a point source of light giving a spherical light distribution, but rather a lambertian distribution. So, LEDs are difficult to apply to uses needing a spherical light field. Different fields of light can be manipulated by the application of different optics or "lenses". LEDs cannot provide divergence below a few degrees.

### Reliability

- Shock resistance: LEDs, being solid-state components, are difficult to damage with external shock, unlike fluorescent and incandescent bulbs, which are fragile.
- Thermal runaway: Parallel strings of LEDs will not share current evenly due to the manufacturing tolerances in their forward voltage. Running two or more strings from a single current source may result in LED failure as the devices warm up. If forward voltage binning is not possible, a circuit is required to ensure even distribution of current between parallel strands.
- Slow failure: LEDs mainly fail by dimming over time, rather than the abrupt failure of incandescent bulbs.
- Lifetime: LEDs can have a relatively long useful life. One report estimates 35,000 to 50,000 hours of useful life for white LEDs, though time to complete failure may be shorter or longer. Fluorescent tubes typically are rated at about 10,000 to 25,000 hours, depending partly on the conditions of use, and incandescent light bulbs at 1,000 to 2,000 hours. Several DOE demonstrations have shown that reduced maintenance costs from this extended lifetime, rather than energy savings, is the primary factor in determining the payback period for an LED product.
- Cycling: LEDs are ideal for uses subject to frequent on-off cycling, unlike incandescent and fluorescent lamps that fail faster when cycled often, or high-intensity discharge lamps (HID lamps) that require a long time to warm up to full output and to cool down before they can be lighted again if they are being restarted.
- Temperature dependence: LED performance largely depends on the ambient temperature of the operating environment – or thermal management properties. Overdriving an LED in high ambient temperatures may result in overheating the LED package, eventually leading to device failure. An adequate heat sink is needed to maintain long life. This is especially important in automotive, medical, and military uses where devices must operate over a wide range of temperatures and require low failure rates.

## Manufacturing

LED manufacturing involves multiple steps, including epitaxy, chip processing, chip separation, and packaging.

In a typical LED manufacturing process, encapsulation is performed after probing, dicing, die transfer from wafer to package, and wire bonding or flip chip mounting, perhaps using indium tin oxide, a transparent electrical conductor. In this case, the bond wire(s) are attached to the ITO film that has been deposited in the LEDs.

Flip chip circuit on board (COB) is a technique that can be used to manufacture LEDs.

## Colors and materials

Conventional LEDs are made from a variety of inorganic semiconductor materials. The following table shows the available colors with wavelength range, voltage drop and material:

|   | Color | Wavelength (nm) | Voltage (V) | Semiconductor material |
|---|---|---|---|---|
|   | Infrared | *λ* > 760 | Δ*V* < 1.9 | Gallium arsenide (GaAs) Aluminium gallium arsenide (AlGaAs) |
|   | Red | 610 < *λ* < 760 | 1.63 < Δ*V* < 2.03 | Aluminium gallium arsenide (AlGaAs) Gallium arsenide phosphide (GaAsP) Aluminium gallium indium phosphide (AlGaInP) Gallium(III) phosphide (GaP) |
|   | Orange | 590 < *λ* < 610 | 2.03 < Δ*V* < 2.10 | Gallium arsenide phosphide (GaAsP) Aluminium gallium indium phosphide (AlGaInP) Gallium(III) phosphide (GaP) |
|   | Yellow | 570 < *λ* < 590 | 2.10 < Δ*V* < 2.18 | Gallium arsenide phosphide (GaAsP) Aluminium gallium indium phosphide (AlGaInP) Gallium(III) phosphide (GaP) |
|   | Green | 500 < *λ* < 570 | 1.9 < Δ*V* < 4.0 | Indium gallium nitride (InGaN) / Gallium(III) nitride (GaN) Gallium(III) phosphide (GaP) Aluminium gallium indium phosphide (AlGaInP) Aluminium gallium phosphide (AlGaP) |
|   | Blue | 450 < *λ* < 500 | 2.48 < Δ*V* < 3.7 | Zinc selenide (ZnSe) Indium gallium nitride (InGaN) Silicon carbide (SiC) as substrate Silicon (Si) as substrate — (under development) |
|   | Violet | 400 < *λ* < 450 | 2.76 < Δ*V* < 4.0 | Indium gallium nitride (InGaN) |
|   | Purple | multiple types | 2.48 < Δ*V* < 3.7 | Generally *not* via band gap manipulation: dual blue/red LEDs, blue with red phosphor, or white with purple plastic |
|   | Ultraviolet | *λ* < 400 | 3.1 < Δ*V* < 4.4 | Aluminium nitride (AlN) (210 nm) Aluminium gallium indium nitride (AlGaInN) — (down to 210 nm) Aluminium gallium nitride (AlGaN) Boron nitride (215 nm) Diamond (235 nm) (The three sub-bands of UV, UVA, UVB, and UVC, each have different uses. See black light, UV-B lamps, and ultraviolet germicidal irradiation.) |
|   | White | Broad spectrum | 2.7 < Δ*V* < 3.5 | Blue diode with yellow phosphor or violet/UV diode with multi-color phosphor |

## Applications

LED uses fall into five major categories:

- Visual signals where light goes more or less directly from the source to the human eye, to convey a message or meaning
- Illumination where light is reflected from objects to give a visual response of these objects
- Measuring and interacting with processes involving no human vision
- Narrow-band light sensors where LEDs operate in a reverse-bias mode and respond to incident light, instead of emitting light
- Indoor cultivation, including cannabis.

The application of LEDs in horticulture has revolutionized plant cultivation by providing energy-efficient, customizable lighting solutions that optimize plant growth and development. LEDs offer precise control over light spectra, intensity, and photoperiods, enabling growers to tailor lighting conditions to the specific needs of different plant species and growth stages. This technology enhances photosynthesis, improves crop yields, and reduces energy costs compared to traditional lighting systems. Additionally, LEDs generate less heat, allowing closer placement to plants without risking thermal damage, and contribute to sustainable farming practices by lowering carbon footprints and extending growing seasons in controlled environments. Light spectrum affects growth, metabolite profile, and resistance against fungal phytopathogens of *Solanum lycopersicum* seedlings. LEDs can also be used in micropropagation.

### Indicators and signs

The low energy consumption, low maintenance and small size of LEDs have led to uses as status indicators and displays on a variety of equipment and installations. Large-area LED displays are used as stadium displays, dynamic decorative displays, and dynamic message signs on freeways. Thin, lightweight message displays are used at airports and railway stations, and as destination displays for trains, buses, trams, and ferries.

One-color light is well suited for traffic lights and signals, exit signs, emergency vehicle lighting, ships' navigation lights, and LED-based Christmas lights.

Because of their long life, fast switching times, and visibility in broad daylight due to their high output and focus, LEDs have been used in automotive brake lights and turn signals. The use in brakes improves safety due to a great reduction in the time needed to light fully, or faster rise time, about 200 milliseconds faster than an incandescent bulb. This gives drivers behind more time to react. In a dual intensity circuit (rear markers and brakes), if the LEDs are not pulsed at a fast enough frequency, they can create a phantom array, where ghost images of the LED appear if the eyes quickly scan across the array. White LED headlamps are beginning to appear. Using LEDs has styling advantages because LEDs can form much thinner lights than incandescent lamps with parabolic reflectors.

Due to the relative cheapness of low-output LEDs, they are also used in many temporary uses such as glowsticks and throwies. Artists have also used LEDs for LED art.

### Lighting

With the development of high-efficiency and high-power LEDs, it has become possible to use LEDs in lighting and illumination. To encourage the shift to LED lamps and other high-efficiency lighting, in 2008 the US Department of Energy created the L Prize competition. The Philips Lighting North America LED bulb won the first competition on August 3, 2011, after successfully completing 18 months of intensive field, lab, and product testing.

Efficient lighting is needed for sustainable architecture. As of 2011, some LED bulbs provide up to 150 lm/W and even inexpensive low-end models typically exceed 50 lm/W, so that a 6-watt LED could achieve the same results as a standard 40-watt incandescent bulb. The lower heat output of LEDs also reduces demand on air conditioning systems. Worldwide, LEDs are rapidly adopted to displace less effective sources such as incandescent lamps and CFLs and reduce electrical energy consumption and its associated emissions. Solar-powered LEDs are used as street lights and in architectural lighting.

The mechanical robustness and long lifetime are used in automotive lighting on cars, motorcycles, and bicycle lights. LED street lights are employed on poles and in parking garages. In 2007, the Italian village of Torraca was the first place to convert its street lighting to LEDs.

Cabin lighting on recent Airbus and Boeing jetliners uses LED lighting. LEDs are also being used in airport and heliport lighting. LED airport fixtures currently include medium-intensity runway lights, runway centerline lights, taxiway centerline and edge lights, guidance signs, and obstruction lighting.

LEDs are also used as a light source for DLP projectors, and to backlight newer LCD televisions (referred to as LED TV), computer monitors (including laptop) and handheld device LCDs, succeeding older CCFL-backlit LCDs although being superseded by OLED screens. RGB LEDs raise the color gamut by as much as 45%. Screens for TV and computer displays can be made thinner using LEDs for backlighting.

LEDs are small, durable and need little power, so they are used in handheld devices such as flashlights. LED strobe lights or camera flashes operate at a safe, low voltage, instead of the 250+ volts commonly found in xenon flashlamp-based lighting. This is especially useful in cameras on mobile phones, where space is at a premium and bulky voltage-raising circuitry is undesirable.

LEDs are used for infrared illumination in night vision uses including security cameras. A ring of LEDs around a video camera, aimed forward into a retroreflective background, allows chroma keying in video productions.

LEDs are used in mining operations as cap lamps to provide light for miners. Research has been done to improve LEDs for mining, to reduce glare and to increase illumination, reducing risk of injury to the miners.

LEDs are increasingly finding uses in medical and educational applications, for example as mood enhancement. NASA has even sponsored research for the use of LEDs to promote health for astronauts.

### Data communication and other signaling

Light can be used to transmit data and analog signals. For example, lighting white LEDs can be used in systems assisting people to navigate in closed spaces while searching necessary rooms or objects.

Assistive listening devices in many theaters and similar spaces use arrays of infrared LEDs to send sound to listeners' receivers. Light-emitting diodes (as well as semiconductor lasers) are used to send data over many types of fiber optic cable, from digital audio over TOSLINK cables to the very high bandwidth fiber links that form the Internet backbone. For some time, computers were commonly equipped with IrDA interfaces, which allowed them to send and receive data to nearby machines via infrared.

Because LEDs can cycle on and off millions of times per second, very high data bandwidth can be achieved. For that reason, visible light communication (VLC) has been proposed as an alternative to the increasingly competitive radio bandwidth. VLC operates in the visible part of the electromagnetic spectrum, so data can be transmitted without occupying the frequencies of radio communications.

### Machine vision systems

Machine vision systems often require bright and homogeneous illumination, so features of interest are easier to process. LEDs are often used.

Barcode scanners are the most common example of machine vision applications, and many of those scanners use red LEDs instead of lasers. Optical computer mice use LEDs as a light source for the miniature camera within the mouse.

LEDs are useful for machine vision because they provide a compact, reliable source of light. LED lamps can be turned on and off to suit the needs of the vision system, and the shape of the beam produced can be tailored to match the system's requirements.

### Biological detection

The discovery of radiative recombination in aluminum gallium nitride (AlGaN) alloys by U.S. Army Research Laboratory (ARL) led to the conceptualization of UV light-emitting diodes (LEDs) to be incorporated in light-induced fluorescence sensors used for biological agent detection. In 2004, the Edgewood Chemical Biological Center (ECBC) initiated the effort to create a biological detector named TAC-BIO. The program capitalized on semiconductor UV optical sources (SUVOS) developed by the Defense Advanced Research Projects Agency (DARPA).

UV-induced fluorescence is one of the most robust techniques used for rapid real-time detection of biological aerosols. The first UV sensors were lasers lacking in-field-use practicality. In order to address this, DARPA incorporated SUVOS technology to create a low-cost, small, lightweight, low-power device. The TAC-BIO detector's response time was one minute from when it sensed a biological agent. It was also demonstrated that the detector could be operated unattended indoors and outdoors for weeks at a time.

Aerosolized biological particles fluoresce and scatter light under a UV light beam. Observed fluorescence is dependent on the applied wavelength and the biochemical fluorophores within the biological agent. UV-induced fluorescence offers a rapid, accurate, efficient and logistically practical way for biological agent detection. This is because the use of UV fluorescence is reagentless, or a process that does not require an added chemical to produce a reaction, with no consumables, or produces no chemical byproducts.

Additionally, TAC-BIO can reliably discriminate between threat and non-threat aerosols. It was claimed to be sensitive enough to detect low concentrations, but not so sensitive that it would cause false positives. The particle-counting algorithm used in the device converted raw data into information by counting the photon pulses per unit of time from the fluorescence and scattering detectors, and comparing the value to a set threshold.

The original TAC-BIO was introduced in 2010, while the second-generation TAC-BIO GEN II, was designed in 2015 to be more cost-efficient, as plastic parts were used. Its small, lightweight design allows it to be mounted to vehicles, robots, and unmanned aerial vehicles. The second-generation device could also be utilized as an environmental detector to monitor air quality in hospitals, airplanes, or even in households to detect fungus and mold.

### Other applications

The light from LEDs can be modulated very quickly so they are used extensively in optical fiber and free-space optics communications. This includes remote controls, such as for television sets, where infrared LEDs are often used. Opto-isolators use an LED combined with a photodiode or phototransistor to provide a signal path with electrical isolation between two circuits. This is especially useful in medical equipment where the signals from a low-voltage sensor circuit (usually battery-powered) in contact with a living organism must be electrically isolated from any possible electrical failure in a recording or monitoring device operating at potentially dangerous voltages. An opto-isolator also lets information be transferred between circuits that do not share a common ground potential.

Many sensor systems rely on light as the signal source. LEDs are often ideal as a light source due to the requirements of the sensors. The Nintendo Wii's sensor bar uses infrared LEDs. Pulse oximeters use them for measuring oxygen saturation. Some flatbed scanners use arrays of RGB LEDs rather than the typical cold-cathode fluorescent lamp as the light source. Having independent control of three illuminated colors allows the scanner to calibrate itself for more accurate color balance, and there is no need for warm-up. Further, its sensors need only be monochromatic, since at any one time the page being scanned is only lit by one color of light.

Since LEDs can also be used as photodiodes, they can be used for both photo emission and detection. This could be used, for example, in a touchscreen that registers reflected light from a finger or stylus. Many materials and biological systems are sensitive to, or dependent on, light. Grow lights use LEDs to increase photosynthesis in plants, and bacteria and viruses can be removed from water and other substances using UV LEDs for sterilization. LEDs of certain wavelengths have also been used for light therapy treatment of neonatal jaundice and acne.

UV LEDs, with spectra range of 220 nm to 395 nm, have other applications, such as water/air purification, surface disinfection, glue curing, free-space non-line-of-sight communication, high-performance liquid chromatography, UV curing dye printing, phototherapy (295 nm Vitamin D, 308 nm Excimer lamp or laser replacement), medical/ analytical instrumentation, and DNA absorption.

LEDs have also been used as a medium-quality voltage reference in electronic circuits. The forward voltage drop (about 1.7 V for a red LED or 1.2V for an infrared) can be used instead of a Zener diode in low-voltage regulators. Red LEDs have the flattest I/V curve above the knee. Nitride-based LEDs have a fairly steep I/V curve and are useless for this purpose. Although LED forward voltage is far more current-dependent than a Zener diode, Zener diodes with breakdown voltages below 3 V are not widely available.

The progressive miniaturization of low-voltage lighting technology, such as LEDs and OLEDs, suitable to incorporate into low-thickness materials has fostered experimentation in combining light sources and wall covering surfaces for interior walls in the form of LED wallpaper.

## Research and development

### Key challenges

LEDs require optimized efficiency to hinge on ongoing improvements such as phosphor materials and quantum dots.

The process of down-conversion (the method by which materials convert more-energetic photons to different, less energetic colors) also needs improvement. For example, the red phosphors that are used today are thermally sensitive and need to be improved in that aspect so that they do not color shift and experience efficiency drop-off with temperature. Red phosphors could also benefit from a narrower spectral width to emit more lumens and become more efficient at converting photons.

In addition, work remains to be done in the realms of current efficiency droop, color shift, system reliability, light distribution, dimming, thermal management, and power supply performance.

Early suspicions were that the LED droop was caused by elevated temperatures. Scientists showed that temperature was not the root cause of efficiency droop. The mechanism causing efficiency droop was identified in 2007 as Auger recombination, which was taken with mixed reaction. A 2013 study conclusively identified Auger recombination as the cause.

### Potential technology

A new family of LEDs are based on the semiconductors called perovskites. In 2018, less than four years after their discovery, the ability of perovskite LEDs (PLEDs) to produce light from electrons already rivaled those of the best performing OLEDs. They have a potential for cost-effectiveness as they can be processed from solution, a low-cost and low-tech method, which might allow perovskite-based devices that have large areas to be made with extremely low cost. Their efficiency is superior by eliminating non-radiative losses, in other words, elimination of recombination pathways that do not produce photons; or by solving outcoupling problem (prevalent for thin-film LEDs) or balancing charge carrier injection to increase the EQE (external quantum efficiency). The most up-to-date PLED devices have broken the performance barrier by shooting the EQE above 20%.

In 2018, Cao et al. and Lin et al. independently published two papers on developing perovskite LEDs with EQE greater than 20%, which made these two papers a milestone in PLED development. Their devices have a similar planar structure, i.e., the active layer (perovskite) is sandwiched between two electrodes. To achieve a high EQE, they not only reduced non-radiative recombination, but also utilized their own, subtly different methods to improve the EQE.

In the work of Cao *et al.*, researchers targeted the outcoupling problem, which is that the optical physics of thin-film LEDs causes the majority of light generated by the semiconductor to be trapped in the device. To achieve this goal, they demonstrated that solution-processed perovskites can spontaneously form submicrometre-scale crystal platelets, which can efficiently extract light from the device. These perovskites are formed via the introduction of amino acid additives into the perovskite precursor solutions. In addition, their method is able to passivate perovskite surface defects and reduce nonradiative recombination. Therefore, by improving the light outcoupling and reducing nonradiative losses, Cao and his colleagues successfully achieved PLED with EQE up to 20.7%.

Lin and his colleague used a different approach to generate high EQE. Instead of modifying the microstructure of the perovskite layer, they chose to adopt a new strategy for managing the compositional distribution in the device—an approach that simultaneously provides high luminescence and balanced charge injection. In other words, they still used a flat emissive layer, but tried to optimize the balance of electrons and holes injected into the perovskite, so as to make the most efficient use of the charge carriers. Moreover, in the perovskite layer, the crystals are perfectly enclosed by MABr additive (where MA is CH3NH3). The MABr shell passivates the nonradiative defects that would otherwise be present in perovskite crystals, resulting in a reduction of nonradiative recombination. Therefore, by balancing charge injection and decreasing nonradiative losses, Lin and his colleagues developed PLED with EQE up to 20.3%.

## Health and safety

Certain blue LEDs and cool-white LEDs can exceed safe limits of the so-called blue-light hazard as defined in eye safety specifications such as "ANSI/IESNA RP-27.1–05: Recommended Practice for Photobiological Safety for Lamp and Lamp Systems". One study showed no evidence of a risk in normal use at domestic illuminance, and that caution is only needed for particular occupational situations or for specific populations. In 2006, the International Electrotechnical Commission published *IEC 62471 Photobiological safety of lamps and lamp systems*, replacing the application of early laser-oriented standards for classification of LED sources.

While LEDs have the advantage over fluorescent lamps, in that they do not contain mercury, they may contain other hazardous metals such as lead and arsenic.

In 2016, the American Medical Association (AMA) issued a statement concerning the possible adverse influence of bluish street lighting on the sleep-wake cycle of city-dwellers. Critics in the industry claim exposure levels are not high enough to have a noticeable effect.
