---
id: electronics/wp2-flux-metallurgy
title: Flux (metallurgy)
category: electronics
tags: [flux, electronics]
summary: In metallurgy, a flux is a chemical reducing agent, flowing agent, or purifying agent.
source: https://en.wikipedia.org/w/index.php?title=Flux_(metallurgy)&oldid=1381042202
source_title: Wikipedia: Flux (metallurgy)
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Flux (metallurgy)", Wikipedia, The Free Encyclopedia (revision 1381042202), https://en.wikipedia.org/w/index.php?title=Flux_(metallurgy)&oldid=1381042202 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Flux (metallurgy)

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In metallurgy, a **flux** is a chemical reducing agent, flowing agent, or purifying agent. Fluxes may have more than one function at a time. They are used in both extractive metallurgy and metal joining. They are named for the ability to make molten metals easier to flow during smelting.

Some of the earliest known fluxes were sodium carbonate, potash, charcoal, coke, borax, lime, lead sulfide and certain minerals containing phosphorus. Iron ore was also used as a flux in the smelting of copper. These agents served various functions, the simplest being a reducing agent, which prevented oxides from forming on the surface of the molten metal, while others absorbed impurities into slag, which could be scraped off molten metal.

Fluxes are also used in foundries for removing impurities from molten nonferrous metals such as aluminium, or for adding desirable trace elements such as titanium.

As reducing agents, fluxes facilitate soldering, brazing, and welding by removing oxidation from the metals to be joined. In some applications, molten flux also serves as a heat-transfer medium, facilitating heating of the joint by the soldering tool.

## Uses

### Metal joining

In high-temperature metal joining processes (welding, brazing and soldering), fluxes are nearly inert at room temperature, but become strongly reducing at elevated temperatures, preventing oxidation of the base and filler materials. The role of flux is typically dual: dissolving the oxides already present on the metal surface to facilitate wetting by molten metal, and acting as an oxygen barrier by coating the hot surface, preventing oxidation.

For example, tin-lead solder attaches very well to copper metal, but poorly to its oxides, which form quickly at soldering temperatures. By preventing the formation of metal oxides, flux enables the solder to adhere to the clean metal surface, rather than forming beads, as it would on an oxidized surface.

#### Soldering

In soldering metals, flux serves a threefold purpose: it removes any oxidized metal from the surfaces to be soldered, seals out air thus preventing further oxidation, and improves the wetting characteristics of the liquid solder. Some fluxes are corrosive, so the parts must be cleaned with a damp sponge or other absorbent material after soldering to prevent damage. Several types of flux are used in electronics.

A number of standards exist to define the various flux types. The principal standard is J-STD-004.

Various tests, including the ROSE test, may be used after soldering to check for the presence of ionic or other contaminants that could cause short circuits or other problems.

#### Brazing and silver soldering

Brazing (sometimes known as silver soldering or hard soldering) requires a higher temperature than soft soldering (> 450 °C). As well as removing existing oxides, rapid oxidation of the metal at the elevated temperatures must be avoided. This means that fluxes must be more aggressive and to provide a physical barrier. Traditionally borax was used as a flux for brazing, but there are now many different fluxes available, often using active chemicals such as fluorides as well as wetting agents. Many of these chemicals are toxic and due care should be taken during their use.

### Smelting

In the process of smelting, inorganic chlorides, fluorides (see fluorite), limestone and other materials are designated as "fluxes" when added to the contents of a smelting furnace or a cupola for the purpose of purging the metal of chemical impurities such as phosphorus, and of rendering slag more liquid at the smelting temperature. Slag is a liquid mixture of ash, flux, and other impurities. This reduction of slag viscosity with temperature, increasing the flow of slag in smelting, is the origin of the word *flux* in metallurgy.

The flux most commonly used in iron and steel furnaces is limestone, which is charged in the proper proportions with the iron and fuel.

## Drawbacks

Fluxes have several serious drawbacks:

- Corrosivity, which is mostly due to the aggressive compounds of the activators; hygroscopic properties of the flux residues may aggravate the effects
- Interference with test equipment, which is due to the insulating residues deposited on the test contacts on electronic circuit boards
- Interference with machine vision systems when the layer of flux or its remains is too thick or improperly located
- Contamination of sensitive parts, e.g. facets of laser diodes, contacts of connectors and mechanical switches, and MEMS assemblies
- Deterioration of electrical properties of printed circuit boards, as soldering temperatures are above the glass transition temperature of the board material and flux components (e.g. glycols, or chloride and bromide ions) can diffuse into its matrix; e.g. water-soluble fluxes containing polyethylene glycol were demonstrated to have such impact
- Deterioration of high-frequency circuit performance by flux residues
- Deterioration of surface insulation resistance, which tends to be as much as three orders of magnitude lower than the bulk resistance of the material
- Electromigration and growth of whiskers between nearby traces, aided by ionic residues, surface moisture and a bias voltage
- The fumes liberated during soldering have adverse health effects, and volatile organic compounds can be outgassed during processing
- The solvents required for post-soldering cleaning of the boards are expensive and may have adverse environmental impact

In special cases the drawbacks are sufficiently serious to warrant using fluxless techniques.

### Dangers

Acid flux types (not used in electronics) may contain hydrochloric acid, zinc chloride or ammonium chloride, which are harmful to humans. Therefore, flux should be handled with gloves and goggles, and used with adequate ventilation.

Prolonged exposure to rosin fumes released during soldering can cause occupational asthma (formerly called *colophony disease* in this context) in sensitive individuals, though which component of the fumes that causes the problem is unknown.

While molten solder has low tendency to adhere to organic materials, molten fluxes, especially of the resin/rosin type, adhere well to fingers. A mass of hot sticky flux can transfer more heat to skin and cause more serious burns than a comparable particle of non-adhering molten metal, which can be quickly shaken off. In this regard, molten flux is similar to molten hot glue.

### Fluxless techniques

In some cases the presence of flux is undesirable; flux traces interfere with e.g. precision optics or MEMS assemblies. Flux residues also tend to outgas in vacuum and space applications, and traces of water, ions and organic compounds may adversely affect long-term reliability of non-hermetic packages. Trapped flux residues are also the cause of most voids in the joints. Flux-less techniques are therefore desirable there.

For successful soldering and brazing, the oxide layer must be removed from both the surfaces of the materials and the surface of the filler metal preform. Exposed surfaces also require protection against oxidation during heating. Flux-coated preforms can also be used to eliminate flux residue entirely from the soldering process.

Protecting surfaces against further oxidation is relatively simple, using vacuum or inert atmosphere. Removal of the native oxide layer is more troublesome, requiring physical or chemical cleaning methods. Surfaces can be protected by, e.g., gold plating. The gold layer must be sufficiently thick and non-porous to provide protection for reasonable storage time. Thick gold metallization also limits choice of soldering alloys, as tin-based solders dissolve gold and form brittle intermetallics, embrittling the joint. Thicker gold coatings are usually limited to use with indium-based solders and solders with high gold content.

Removal of the oxides from the solder preform is also troublesome. Fortunately some alloys are able to dissolve the surface oxides in their bulk when superheated by several degrees above their melting point; the Sn-Cu₁ and Sn-Ag₄ require superheating by 18–19 °C, the Sn-Sb₅ requires as little as 10 °C, but the Sn-Pb₃₇ alloy requires 77 °C above its melting point to dissolve its surface oxide. The self-dissolved oxide degrades the solder's properties and increases its viscosity in molten state, however, so this approach is not optimal.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Drawbacks, Properties, Composition, Rosin fluxes, Special fluxes, Flux recovery.)_
