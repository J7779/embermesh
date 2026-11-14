---
id: radio/neets10-ch2-radio-wave-propagation-2
title: "NEETS Module 10 Ch. 2: Radio Wave Propagation (2/2)"
category: radio
tags: [radio, propagation, ionosphere, MUF, fading, ducting]
summary: "Navy training text on how radio waves travel: ground and sky waves, the ionosphere, MUF/LUF/FOT, fading and tropospheric effects. (continued)"
source: "https://archive.org/details/NEETSModule10"
source_title: "Navy Electricity and Electronics Training Series (NEETS) Module 10: Introduction to Wave Propagation, Transmission Lines, and Antennas (NAVEDTRA 14182)"
license: "Public Domain (US Government work, 17 U.S.C. §105)"
attribution: "Naval Education and Training Professional Development and Technology Center, US Navy. US Government work; 'Approved for public release; distribution is unlimited.' Figures not reproduced; end-of-chapter question answers retained."
retrieved: 2026-11-13
part_of: radio/neets10
order: 22
---

# NEETS Module 10 Ch. 2: Radio Wave Propagation (2/2)

## Absorption In The Ionosphere

Many factors affect a radio wave in its path between the transmitting and receiving sites. The factor that has the greatest adverse effect on radio waves is ABSORPTION. Absorption results in the loss of energy of a radio wave and has a pronounced effect on both the strength of received signals and the ability to communicate over long distances.

You learned earlier in the section on ground waves that surface waves suffer most of their absorption losses because of ground-induced voltage. Sky waves, on the other hand, suffer most of their absorption losses because of conditions in the ionosphere. Note that some absorption of sky waves may also occur at lower atmospheric levels because of the presence of water and water vapor. However, this becomes important only at frequencies above 10,000 megahertz.

Most ionospheric absorption occurs in the lower regions of the ionosphere where ionization density is greatest. As a radio wave passes into the ionosphere, it loses some of its energy to the free electrons and ions. If these high-energy free electrons and ions do not collide with gas molecules of low energy, most of the energy lost by the radio wave is reconverted into electromagnetic energy, and the wave continues to be propagated with little change in intensity. However, if the high-energy free electrons and ions do collide with other particles, much of this energy is lost, resulting in absorption of the energy from the wave. Since absorption of energy depends on collision of the particles, the greater the density of the ionized layer, the greater the probability of collisions; therefore, the greater the absorption. The highly dense D and E layers provide the greatest absorption of radio waves.

Because the amount of absorption of the sky wave depends on the density of the ionosphere, which varies with seasonal and daily conditions, it is impossible to express a fixed relationship between distance and signal strength for ionospheric propagation. Under certain conditions, the absorption of energy is so great that communicating over any distance beyond the line of sight is difficult.

### Fading

The most troublesome and frustrating problem in receiving radio signals is variations in signal strength, most commonly known as FADING. There are several conditions that can produce fading. When a radio wave is refracted by the ionosphere or reflected from the Earth's surface, random changes in the polarization of the wave may occur. Vertically and horizontally mounted receiving antennas are designed to receive vertically and horizontally polarized waves, respectively. Therefore, changes in polarization cause changes in the received signal level because of the inability of the antenna to receive polarization changes.

Fading also results from absorption of the rf energy in the ionosphere. Absorption fading occurs for a longer period than other types of fading, since absorption takes place slowly.

Usually, however, fading on ionospheric circuits is mainly a result of multipath propagation. Multipath Fading MULTIPATH is simply a term used to describe the multiple paths a radio wave may follow between transmitter and receiver. Such propagation paths include the ground wave, ionospheric refraction, reradiation by the ionospheric layers, reflection from the Earth's surface or from more than one ionospheric layer, etc. Figure 2-21 shows a few of the paths that a signal can travel between two sites in a typical circuit. One path, XYZ, is the basic ground wave. Another path, XEA, refracts the wave at the E layer and passes it on to the receiver at A. Still another path, XFZFA, results from a greater angle of incidence and two refractions from the F layer. At point Z, the received signal is a combination of the ground wave and the sky wave. These two signals having traveled different paths arrive at point Z at different times. Thus, the arriving waves may or may not be in phase with each other. Radio waves that are received in phase reinforce each other and produce a stronger signal at the receiving site. Conversely, those that are received out of phase produce a weak or fading signal. Small alternations in the transmission path may change the phase relationship of the two signals, causing periodic fading. This condition occurs at point A. At this point, the double-hop F layer signal may be in or out of phase with the signal arriving from the E layer.

*[Figure 2-21: Multipath transmission (figure not reproduced).]*

Multipath fading may be minimized by practices called SPACE DIVERSITY and FREQUENCY DIVERSITY. In space diversity, two or more receiving antennas are spaced some distance apart. Fading does not occur simultaneously at both antennas; therefore, enough output is almost always available from one of the antennas to provide a useful signal. In frequency diversity, two transmitters and two receivers are used, each pair tuned to a different frequency, with the same information being transmitted simultaneously over both frequencies. One of the two receivers will almost always provide a useful signal.

Selective Fading Fading resulting from multipath propagation is variable with frequency since each frequency arrives at the receiving point via a different radio path. When a wide band of frequencies is transmitted simultaneously, each frequency will vary in the amount of fading. This variation is called SELECTIVE FADING. When selective fading occurs, all frequencies of the transmitted signal do not retain their original phases and relative amplitudes. This fading causes severe distortion of the signal and limits the total signal transmitted.

**Q23.** What is the skip zone of a radio wave?

**Q24.** Where does the greatest amount of ionospheric absorption occur in the ionosphere?

**Q25.** What is meant by the term "multipath"?

**Q26.** When a wide band of frequencies is transmitted simultaneously, each frequency will vary in the amount of fading. What is this variable fading called?

### Transmission Losses

All radio waves propagated over ionospheric paths undergo energy losses before arriving at the receiving site. As we discussed earlier, absorption in the ionosphere and lower atmospheric levels account for a large part of these energy losses. There are two other types of losses that also significantly affect the ionospheric propagation of radio waves. These losses are known as ground reflection loss and free space loss. The combined effects of absorption, ground reflection loss, and free space loss account for most of the energy losses of radio transmissions propagated by the ionosphere.

Ground Reflection Loss When propagation is accomplished via multihop refraction, rf energy is lost each time the radio wave is reflected from the Earth's surface. The amount of energy lost depends on the frequency of the wave, the angle of incidence, ground irregularities, and the electrical conductivity of the point of reflection. Free space Loss Normally, the major loss of energy is because of the spreading out of the wavefront as it travels away from the transmitter. As the distance increases, the area of the wavefront spreads out, much like the beam of a flashlight. This means the amount of energy contained within any unit of area on the wavefront will decrease as distance increases. By the time the energy arrives at the receiving antenna, the wavefront is so spread out that the receiving antenna extends into only a very small fraction of the wavefront. This is illustrated in figure 2-22.

*[Figure 2-22: Free space loss principle (figure not reproduced).]*

### Electromagnetic Interference (Emi)

The transmission losses just discussed are not the only factors that interfere with communications. An additional factor that can interfere with radio communications is the presence of ELECTROMAGNETIC INTERFERENCE (EMI). This interference can result in annoying or impossible operating conditions. Sources of emi are both man-made and natural.

Man-Made Interference Man-made interference may come from several sources. Some of these sources, such as oscillators, communications transmitters, and radio transmitters, may be specifically designed to generate radio frequency energy. Some electrical devices also generate radio frequency energy, although they are not specifically designed for this purpose. Examples are ignition systems, generators, motors, switches, relays, and voltage regulators. The intensity of man-made interference may vary throughout the day and drop off to a low level at night when many of these sources are not being used. Man-made interference may be a critical limiting factor at radio receiving sites located near industrial areas. Natural Interference Natural interference refers to the static that you often hear when listening to a radio. This interference is generated by natural phenomena, such as thunderstorms, snowstorms, cosmic sources, and the sun. The energy released by these sources is transmitted to the receiving site in roughly the same manner as radio waves. As a result, when ionospheric conditions are favorable for the long distance propagation of radio waves, they are likewise favorable for the propagation of natural interference. Natural interference is very erratic, particularly in the hf band, but generally will decrease as the operating frequency is increased and wider bandwidths are used. There is little natural interference above 30 megahertz.

Control of EMI Electromagnetic interference can be reduced or eliminated by using various suppression techniques. The amount of emi that is produced by a radio transmitter can be controlled by cutting transmitting antennas to the correct frequency, limiting bandwidth, and using electronic filtering networks and metallic shielding.

Radiated emi during transmission can be controlled by the physical separation of the transmitting and receiving antennas, the use of directional antennas, and limiting antenna bandwidth.

**Q27.** What are the two main sources of emi with which radio waves must compete?

**Q28.** Thunderstorms, snowstorms, cosmic sources, the sun, etc., are a few examples of emi sources. What type of emi comes from these sources?

**Q29.** Motors, switches, voltage regulators, generators, etc., are a few examples of emi sources. What type of emi comes from these sources?

**Q30.** What are three ways of controlling the amount of transmitter-generated emi?

**Q31.** What are three ways of controlling radiated emi during transmission?

### Variations In The Ionosphere

Because the existence of the ionosphere is directly related to radiations emitted from the sun, the movement of the Earth about the sun or changes in the sun's activity will result in variations in the ionosphere. These variations are of two general types: (1) those which are more or less regular and occur in cycles and, therefore, can be predicted in advance with reasonable accuracy, and (2) those which are irregular as a result of abnormal behavior of the sun and, therefore, cannot be predicted in advance. Both regular and irregular variations have important effects on radio wave propagation.

Regular Variations The regular variations that affect the extent of ionization in the ionosphere can be divided into four main classes: daily, seasonal, 11-year, and 27-day variations.

DAILY.—Daily variations in the ionosphere are a result of the 24-hour rotation of the Earth about its axis. Daily variations of the different layers (fig. 2-14) are summarized as follows:

- The D layer reflects vlf waves; is important for long range vlf communications; refracts lf and mf waves for short range communications; absorbs hf waves; has little effect on vhf and above; and disappears at night.
- In the E layer, ionization depends on the angle of the sun. The E layer refracts hf waves during the day up to 20 megahertz to distances of about 1200 miles. Ionization is greatly reduced at night.
- Structure and density of the F region depend on the time of day and the angle of the sun. This region consists of one layer during the night and splits into two layers during daylight hours.
- Ionization density of the F1 layer depends on the angle of the sun. Its main effect is to absorb hf waves passing through to the F2 layer.
- The F2 layer is the most important layer for long distance hf communications. It is a very variable layer and its height and density change with time of day, season, and sunspot activity. SEASONAL.—Seasonal variations are the result of the Earth revolving around the sun; the relative position of the sun moves from one hemisphere to the other with changes in seasons. Seasonal variations of the D, E, and F1 layers correspond to the highest angle of the sun; thus the ionization density of these layers is greatest during the summer. The F2 layer, however, does not follow this pattern; its ionization is greatest in winter and least in summer, the reverse of what might be expected. As a result, operating frequencies for F2 layer propagation are higher in the winter than in the summer.

ELEVEN-YEAR SUN SPOT CYCLE.—One of the most notable phenomena on the surface of the sun is the appearance and disappearance of dark, irregularly shaped areas known as SUNSPOTS. The exact nature of sunspots is not known, but scientists believe they are caused by violent eruptions on the sun and are characterized by unusually strong magnetic fields. These sunspots are responsible for variations in the ionization level of the ionosphere. Sunspots can, of course, occur unexpectedly, and the life span of individual sunspots is variable; however, a regular cycle of sunspot activity has also been observed. This cycle has both a minimum and maximum level of sunspot activity that occur approximately every 11 years.

During periods of maximum sunspot activity, the ionization density of all layers increases. Because of this, absorption in the D layer increases and the critical frequencies for the E, F1, and F2 layers are higher. At these times, higher operating frequencies must be used for long distance communications. 27-DAY SUNSPOT CYCLE.—The number of sunspots in existence at any one time is continually subject to change as some disappear and new ones emerge. As the sun rotates on its own axis, these sunspots are visible at 27-day intervals, the approximate period required for the sun to make one complete rotation.

The 27-day sunspot cycle causes variations in the ionization density of the layers on a day-to-day basis. The fluctuations in the F2 layer are greater than for any other layer. For this reason, precise predictions on a day-to-day basis of the critical frequency of the F2 layer are not possible. In calculating frequencies for long-distance communications, allowances for the fluctuations of the F2 layer must be made.

Irregular Variations Irregular variations in ionospheric conditions also have an important effect on radio wave propagation. Because these variations are irregular and unpredictable, they can drastically affect communications capabilities without any warning.

The more common irregular variations are sporadic E, sudden ionospheric disturbances, and ionospheric storms.

SPORADIC E.—Irregular cloud-like patches of unusually high ionization, called sporadic E, often form at heights near the normal E layer. Exactly what causes this phenomenon is not known, nor can its occurrence be predicted. It is known to vary significantly with latitude, and in the northern latitudes, it appears to be closely related to the aurora borealis or northern lights.

At times the sporadic E is so thin that radio waves penetrate it easily and are returned to earth by the upper layers. At other times, it extends up to several hundred miles and is heavily ionized. These characteristics may be either harmful or helpful to radio wave propagation. For example, sporadic E may blank out the use of higher, more favorable ionospheric layers or cause additional absorption of the radio wave at some frequencies. Also, it can cause additional multipath problems and delay the arrival times of the rays of rf energy.

On the other hand, the critical frequency of the sporadic E is very high and can be greater than double the critical frequency of the normal ionospheric layers. This condition may permit the long distance transmission of signals at unusually high frequencies. It may also permit short distance communications to locations that would normally be in the skip zone.

The sporadic E can form and disappear in a short time during either the day or night. However, it usually does not occur at the same time at all transmitting or receiving stations.

SUDDEN IONOSPHERIC DISTURBANCES.—The most startling of the ionospheric irregularities is known as a SUDDEN IONOSPHERIC DISTURBANCE (sid). These disturbances may occur without warning and may prevail for any length of time, from a few minutes to several hours. When sid occurs, long distance propagation of hf radio waves is almost totally "blanked out." The immediate effect is that radio operators listening on normal frequencies are inclined to believe their receivers have gone dead.

When sid has occurred, examination of the sun has revealed a bright solar eruption. All stations lying wholly, or in part, on the sunward side of the Earth are affected. The solar eruption produces an unusually intense burst of ultraviolet light, which is not absorbed by the F2, F1, and E layers, but instead causes a sudden abnormal increase in the ionization density of the D layer. As a result, frequencies above 1 or 2 megahertz are unable to penetrate the D layer and are usually completely absorbed by the layer. IONOSPHERIC STORMS.—Ionospheric storms are disturbances in the Earth's magnetic field. They are associated, in a manner not fully understood, with both solar eruptions and the 27-day intervals, thus corresponding to the rotation of the sun.

Scientists believe that ionospheric storms result from particle radiation from the sun. Particles radiated from a solar eruption have a slower velocity than ultraviolet light waves produced by the eruption. This would account for the 18-hour or so time difference between a sid and an ionospheric storm. An ionospheric storm that is associated with sunspot activity may begin anytime from 2 days before an active sunspot crosses the central meridian of the sun until four days after it passes the central meridian. At times, however, active sunspots have crossed the central region of the sun without any ionospheric storms occurring. Conversely, ionospheric storms have occurred when there were no visible spots on the sun and no preceding sid. As you can see, some correlation between ionospheric storms, sid, and sunspot activity is possible, but there are no hard and fast rules. Ionospheric storms can occur suddenly without warning.

The most prominent effects of ionospheric storms are a turbulent ionosphere and very erratic sky wave propagation. Critical frequencies are lower than normal, particularly for the F2 layer. Ionospheric storms affect the higher F2 layer first, reducing its ion density. Lower layers are not appreciably affected by the storms unless the disturbance is great. The practical effect of ionospheric storms is that the range of frequencies that can be used for communications on a given circuit is much smaller than normal, and communications are possible only at the lower working frequencies.

**Q32.** What are the two general types of variations in the ionosphere?

**Q33.** What is the main difference between these two types of variations?

**Q34.** What are the four main classes of regular variation which affect the extent of ionization in the ionosphere?

**Q35.** What are the three more common types of irregular variations in the ionosphere?

### Frequency Selection Considerations

Up to this point, we have covered various factors that control the propagation of radio waves through the ionosphere, such as the structure of the ionosphere, the incidence angle of radio waves, operating frequencies, etc. There is a very good reason for studying radio wave propagation. You must have a thorough knowledge of radio wave propagation to exercise good judgment when you select transmitting and receiving antennas and operating frequencies. Selection of a suitable operating frequency (within the bounds of frequency allocations and availability) is of prime importance in maintaining reliable communications.

For successful communications between any two specified locations at any given time of the day, there is a maximum frequency, a lowest frequency, and an optimum frequency that can be used. Maximum Usable Frequency As we discussed earlier, the higher the frequency of a radio wave, the lower the rate of refraction by an ionized layer. Therefore, for a given angle of incidence and time of day, there is a maximum frequency that can be used for communications between two given locations. This frequency is known as the MAXIMUM USABLE FREQUENCY (muf).

Waves at frequencies above the muf are normally refracted so slowly that they return to Earth beyond the desired location, or pass on through the ionosphere and are lost. You should understand, however, that use of an established muf certainly does not guarantee successful communications between a transmitting site and a receiving site. Variations in the ionosphere may occur at any time and consequently raise or lower the predetermined muf. This is particularly true for radio waves being refracted by the highly variable F2 layer.

The muf is highest around noon when ultraviolet light waves from the sun are the most intense. It then drops rather sharply as recombination begins to take place.

Lowest Usable Frequency As there is a maximum operating frequency that can be used for communications between two points, there is also a minimum operating frequency. This is known as the LOWEST USABLE FREQUENCY (luf).

As the frequency of a radio wave is lowered, the rate of refraction increases. So a wave whose frequency is below the established luf is refracted back to Earth at a shorter distance than desired, as shown in figure 2-23.

*[Figure 2-23: Refraction of frequency below the lowest usable frequency (luf) (figure not reproduced).]*

The transmission path that results from the rate of refraction is not the only factor that determines the luf. As a frequency is lowered, absorption of the radio wave increases. A wave whose frequency is too low is absorbed to such an extent that it is too weak for reception. Likewise, atmospheric noise is greater at lower frequencies; thus, a low-frequency radio wave may have an unacceptable signal-to-noise ratio. For a given angle of incidence and set of ionospheric conditions, the luf for successful communications between two locations depends on the refraction properties of the ionosphere, absorption considerations, and the amount of atmospheric noise present.

Optimum Working Frequency Neither the muf nor the luf is a practical operating frequency. While radio waves at the luf can be refracted back to Earth at the desired location, the signal-to-noise ratio is still much lower than at the higher frequencies, and the probability of multipath propagation is much greater. Operating at or near the muf can result in frequent signal fading and dropouts when ionospheric variations alter the length of the transmission path.

The most practical operating frequency is one that you can rely on with the least amount of problems. It should be high enough to avoid the problems of multipath, absorption, and noise encountered at the lower frequencies; but not so high as to result in the adverse effects of rapid changes in the ionosphere.

A frequency that meets the above criteria has been established and is known as the OPTIMUM WORKING FREQUENCY. It is abbreviated "fot" from the initial letters of the French words for optimum working frequency, "frequence optimum de travail." The fot is roughly about 85 percent of the muf but the actual percentage varies and may be either considerably more or less than 85 percent.

**Q36.** What do the letters muf, luf, and fot stand for?

**Q37.** When is muf at its highest and why?

**Q38.** What happens to the radio wave if the luf is too low?

**Q39.** What are some disadvantages of operating transmitters at or near the luf?

**Q40.** What are some disadvantages of operating a transmitter at or near the muf?

**Q41.** What is fot?

### Weather Versus Propagation

Weather is an additional factor that affects the propagation of radio waves. In this section, we will explain how and to what extent the various weather phenomena affect wave propagation. Wind, air temperature, and water content of the atmosphere can combine in many ways. Certain combinations can cause radio signals to be heard hundreds of miles beyond the ordinary range of radio communications. Conversely, a different combination of factors can cause such attenuation of the signal that it may not be heard even over a normally satisfactory path. Unfortunately, there are no hard and fast rules on the effects of weather on radio transmissions since the weather is extremely complex and subject to frequent change. We will, therefore, limit our discussion on the effects of weather on radio waves to general terms.

### Precipitation Attenuation

Calculating the effect of weather on radio wave propagation would be comparatively simple if there were no water or water vapor in the atmosphere. However, some form of water (vapor, liquid, or solid) is always present and must be considered in all calculations. Before we begin discussing the specific effects that individual forms of precipitation (rain, snow, fog) have on radio waves, you should understand that attenuation because of precipitation is generally proportionate to the frequency and wavelength of the radio wave. For example, rain has a pronounced effect on waves at microwave frequencies. However, rain hardly affects waves with long wavelengths (hf range and below). You can assume, then, that as the wavelength becomes shorter with increases in frequency, precipitation has an increasingly important attenuation effect on radio waves. Conversely, you can assume that as the wavelength becomes longer with decreases in frequency, precipitation has little attenuation effect.

Rain Attenuation because of raindrops is greater than attenuation because of other forms of precipitation. Attenuation may be caused by absorption, in which the raindrop, acting as a poor dielectric, absorbs power from the radio wave and dissipates the power by heat loss or by scattering (fig. 2-24). Raindrops cause greater attenuation by scattering than by absorption at frequencies above 100 megahertz. At frequencies above 6 gigahertz, attenuation by raindrop scatter is even greater.

*[Figure 2-24: Rf energy losses from scattering (figure not reproduced).]*

Fog In the discussion of attenuation, fog may be considered as another form of rain. Since fog remains suspended in the atmosphere, the attenuation is determined by the quantity of water per unit volume and by the size of the droplets. Attenuation because of fog is of minor importance at frequencies lower than 2 gigahertz. However, fog can cause serious attenuation by absorption, at frequencies above 2 gigahertz. Snow The scattering effect because of snow is difficult to compute because of irregular sizes and shapes of the flakes. While information on the attenuating effect of snow is limited, scientists assume that attenuation from snow is less than from rain falling at an equal rate. This assumption is borne out by the fact that the density of rain is eight times the density of snow. As a result, rain falling at 1 inch per hour would have more water per cubic inch than snow falling at the same rate.

Hail Attenuation by hail is determined by the size of the stones and their density. Attenuation of radio waves by scattering because of hailstones is considerably less than by rain.

### Temperature Inversion

Under normal atmospheric conditions, the warmest air is found near the surface of the Earth. The air gradually becomes cooler as altitude increases. At times, however, an unusual situation develops in which layers of warm air are formed above layers of cool air. This condition is known as TEMPERATURE INVERSION. These temperature inversions cause channels, or ducts, of cool air to be sandwiched between the surface of the Earth and a layer of warm air, or between two layers of warm air. If a transmitting antenna extends into such a duct of cool air, or if the radio wave enters the duct at a very low angle of incidence, vhf and uhf transmissions may be propagated far beyond normal line-of-sight distances. When ducts are present as a result of temperature inversions, good reception of vhf and uhf television signals from a station located hundreds of miles away is not unusual. These long distances are possible because of the different densities and refractive qualities of warm and cool air. The sudden change in density when a radio wave enters the warm air above a duct causes the wave to be refracted back toward Earth. When the wave strikes the Earth or a warm layer below the duct, it is again reflected or refracted upward and proceeds on through the duct with a multiple-hop type of action. An example of the propagation of radio waves by ducting is shown in figure 2-25.

*[Figure 2-25: Duct effect caused by temperature inversion (figure not reproduced).]*

**Q42.** How do raindrops affect radio waves?

**Q43.** How does fog affect radio waves at frequencies above 2 gigahertz?

**Q44.** How is the term "temperature inversion" used when referring to radio waves?

**Q45.** How does temperature inversion affect radio transmission?

### Tropospheric Propagation

As the lowest region of the Earth's atmosphere, the troposphere extends from the Earth's surface to a height of slightly over 7 miles. Virtually all weather phenomena occur in this region. Generally, the troposphere is characterized by a steady decrease in both temperature and pressure as height is increased. However, the many changes in weather phenomena cause variations in humidity and an uneven heating of the Earth's surface. As a result, the air in the troposphere is in constant motion. This motion causes small turbulences, or eddies, to be formed, as shown by the bouncing of aircraft entering turbulent areas of the atmosphere. These turbulences are most intense near the Earth's surface and gradually diminish with height. They have a refractive quality that permits the refracting or scattering of radio waves with short wavelengths. This scattering provides enhanced communications at higher frequencies. Recall that in the relationship between frequency and wavelength, wavelength decreases as frequency increases and vice versa. Radio waves of frequencies below 30 megahertz normally have wavelengths longer than the size of weather turbulences. These radio waves are, therefore, affected very little by the turbulences. On the other hand, as the frequency increases into the vhf range and above, the wavelengths decrease in size, to the point that they become subject to tropospheric scattering. The usable frequency range for tropospheric scattering is from about 100 megahertz to 10 gigahertz.

### Tropospheric Scattering

When a radio wave passing through the troposphere meets a turbulence, it makes an abrupt change in velocity. This causes a small amount of the energy to be scattered in a forward direction and returned to Earth at distances beyond the horizon. This phenomenon is repeated as the radio wave meets other turbulences in its path. The total received signal is an accumulation of the energy received from each of the turbulences.

This scattering mode of propagation enables vhf and uhf signals to be transmitted far beyond the normal line-of-sight. To better understand how these signals are transmitted over greater distances, you must first consider the propagation characteristics of the space wave used in vhf and uhf line-of-sight communications. When the space wave is transmitted, it undergoes very little attenuation within the line-of-sight horizon. When it reaches the horizon, the wave is diffracted and follows the Earth's curvature. Beyond the horizon, the rate of attenuation increases very rapidly and signals soon become very weak and unusable.

Tropospheric scattering, on the other hand, provides a usable signal at distances beyond the point where the diffracted space wave drops to an unusable level. This is because of the height at which scattering takes place. The turbulence that causes the scattering can be visualized as a relay station located above the horizon; it receives the transmitted energy and then reradiates it in a forward direction to some point beyond the line-of-sight distance. A high gain receiving antenna aimed toward this scattered energy can then capture it.

The magnitude of the received signal depends on the number of turbulences causing scatter in the desired direction and the gain of the receiving antenna. The scatter area used for tropospheric scatter is known as the scatter volume. The angle at which the receiving antenna must be aimed to capture the scattered energy is called the scatter angle. The scatter volume and scatter angle are shown in figure 2-26.

*[Figure 2-26: Tropospheric scattering propagation (figure not reproduced).]*

The signal take-off angle (transmitting antenna's angle of radiation) determines the height of the scatter volume and the size of the scatter angle. A low signal take-off angle produces a low scatter volume, which in turn permits a receiving antenna that is aimed at a low angle to the scatter volume to capture the scattered energy.

As the signal take-off angle is increased, the height of the scatter volume is increased. When this occurs, the amount of received energy decreases. There are two reasons for this: (1) scatter angle increases as the height of the scatter volume is increased; (2) the amount of turbulence decreases with height. As the distance between the transmitting and receiving antennas is increased, the height of the scatter volume must also be increased. The received signal level, therefore, decreases as circuit distance is increased.

The tropospheric region that contributes most strongly to tropospheric scatter propagation lies near the midpoint between the transmitting and receiving antennas and just above the radio horizon of the antennas.

Since tropospheric scatter depends on turbulence in the atmosphere, changes in atmospheric conditions have an effect on the strength of the received signal. Both daily and seasonal variations in signal strength occur as a result of changes in the atmosphere. These variations are called long-term fading.

In addition to long-term fading, the tropospheric scatter signal often is characterized by very rapid fading because of multipath propagation. Since the turbulent condition is constantly changing, the path lengths and individual signal levels are also changing, resulting in a rapidly changing signal. Although the signal level of the received signal is constantly changing, the average signal level is stable; therefore, no complete fade out occurs.

Another characteristic of a tropospheric scatter signal is its relatively low power level. Since very little of the scattered energy is reradiated toward the receiver, the efficiency is very low and the signal level at the final receiver point is low. Initial input power must be high to compensate for the low efficiency in the scatter volume. This is accomplished by using high-power transmitters and high-gain antennas, which concentrate the transmitted power into a beam, thus increasing the intensity of energy of each turbulence in the volume. The receiver must also be very sensitive to detect the low-level signals.

### Application Of Tropospheric Scattering

Tropospheric scatter propagation is used for point-to-point communications. A correctly designed tropospheric scatter circuit will provide highly reliable service for distances ranging from 50 miles to 500 miles. Tropospheric scatter systems may be particularly useful for communications to locations in rugged terrain that are difficult to reach with other methods of propagation. One reason for this is that the tropospheric scatter circuit is not affected by ionospheric and auroral disturbances.

**Q46.** In what layer of the atmosphere does virtually all weather phenomena occur?

**Q47.** Which radio frequency bands use the tropospheric scattering principle for propagation of radio waves?

**Q48.** Where is the tropospheric region that contributes most strongly to tropospheric scatter propagation?

### Summary

Now that you have completed this chapter, let's review some of the new terms, concepts, and ideas that you have learned. You should have a thorough understanding of these principles before moving on to chapter 3.

The INDUCTION FIELD contains an E field and an H field and is localized near the antenna. The E and H fields of the induction field are 90 degrees out of phase with each other.

The RADIATION FIELD contains E and H fields that are propagated from the antenna into space in the form of electromagnetic waves. The E and H fields of the radiation field are in phase with each other.

A HARMONIC FREQUENCY is any frequency that is a whole number multiple of a smaller basic frequency. For example, a radio wave transmitted at a fundamental frequency of 3000 hertz can have a second harmonic of 6000 hertz, a third harmonic frequency of 9000 hertz, etc., transmitted at the same time.

A VERTICALLY POLARIZED antenna transmits an electromagnetic wave with the E field perpendicular to the Earth's surface. A HORIZONTALLY POLARIZED antenna transmits a radio wave with the E field parallel to the Earth's surface.

A WAVEFRONT is a small section of an expanding sphere of radiated energy and is perpendicular to the direction of travel from the antenna.

RADIO WAVES are electromagnetic waves that can be reflected, refracted, and diffracted in the atmosphere like light and heat waves.

REFLECTED RADIO WAVES are waves that have been reflected from a surface and are 180 degrees out of phase with the initial wave.

The Earth's atmosphere is divided into three separate layers: The TROPOSPHERE, STRATOSPHERE, and IONOSPHERE.

The TROPOSPHERE is the region of the atmosphere where virtually all weather phenomena take place. In this region, rf energy is greatly affected.

The STRATOSPHERE has a constant temperature and has little effect on radio waves.

The IONOSPHERE contains four cloud-like layers of electrically charged ions which aid in long distance communications.

GROUND WAVES and SKY WAVES are the two basic types of radio waves that transmit energy from the transmitting antenna to the receiving antenna.

GROUND WAVES are composed of two separate component waves: the SURFACE WAVE and the SPACE WAVE.

SURFACE WAVES travel along the contour of the Earth by diffraction.

SPACE WAVES can travel through the air directly to the receiving antenna or can be reflected from the surface of the Earth.

SKY WAVES, often called ionospheric waves, are radiated in an upward direction and returned to Earth at some distant location because of refraction.

NATURAL HORIZON is the line-of-sight horizon.

RADIO HORIZON is one-third farther than the natural horizon.

The IONOSPHERE consists of several layers of ions, formed by the process called ionization. IONIZATION is the process of knocking electrons free from their parent atom, thus upsetting electrical neutrality.

RECOMBINATION is the opposite of ionization; that is, the free ions combine with positive ions, causing the positive ions to return to their original neutral atom state.

The D LAYER is the lowest region of the ionosphere and refracts signals of low frequencies back to Earth.

The E LAYER is present during the daylight hours; refracts signals as high as 20 megahertz back to Earth; and is used for communications up to 1500 miles.

The F LAYER is divided into the F1 and F2 layers during the day but combine at night to form one layer. This layer is responsible for high-frequency, long-range transmission.

The CRITICAL FREQUENCY is the maximum frequency that a radio wave can be transmitted vertically and still be refracted back to Earth.

The CRITICAL ANGLE is the maximum and/or minimum angle that a radio wave can be transmitted and still be refracted back to Earth.

SKIP DISTANCE is the distance between the transmitter and the point where the sky wave first returns to Earth.

SKIP ZONE is the zone of silence between the point where the ground wave becomes too weak for reception and the point where the sky wave is first returned to Earth.

FADING is caused by variations in signal strength, such as absorption of the rf energy by the ionosphere.

MULTIPATH FADING occurs when a transmitted signal divides and takes more than one path to a receiver and some of the signals arrive out of phase, resulting in a weak or fading signal. Some TRANSMISSION LOSSES that affect radio-wave propagation are ionospheric absorption, ground reflection, and free-space losses.

ELECTROMAGNETIC INTERFERENCE (emi), both natural and man-made, interfere with radio communications.

The MAXIMUM USABLE FREQUENCY (muf) is the highest frequency that can be used for communications between two locations at a given angle of incidence and time of day.

The LOWEST USABLE FREQUENCY (luf) is the lowest frequency that can be used for communications between two locations.

OPTIMUM WORKING FREQUENCY (fot) is the most practical operating frequency and the one that can be relied on to have the fewest problems.

PRECIPITATION ATTENUATION can be caused by rain, fog, snow, and hail; and can affect overall communications considerably.

TEMPERATURE INVERSION causes channels, or ducts, of cool air to form between layers of warm air, which can cause radio waves to travel far beyond the normal line-of-sight distances. TROPOSPHERIC PROPAGATION uses the scattering principle to achieve beyond the line-of-sight radio communications within the troposphere.

### Answers To Questions Q1. Through Q48.

**A1.** Induction field and radiation field.

**A2.** Induction field.

**A3.** Radiation field.

**A4.** Fundamental frequency.

**A5.** Harmonic frequency or harmonics.

**A6.** 30 meters.

**A7.** 5 megahertz.

**A8.** Vertically polarized.

**A9.** Direction of wave propagation.

**A10.** Shifting in the phase relationships of the wave.

**A11.** Troposphere, stratosphere, and ionosphere.

**A12.** Stratosphere.

**A13.** Whether the component of the wave is travelling along the surface or over the surface of the earth.

**A14.** Radio horizon is about 1/3 farther.

**A15.** Sea water.

**A16.** (a) electrical properties of the terrain (b) frequency (c) polarization of the antenna

**A17.** High energy ultraviolet light waves from the sun.

**A18.** D, E, F1, and F2 layers.

**A19.** D layer is 30-55 miles, E layer 55-90 miles, and F layers are 90-240 miles.

**A20.** Thickness of ionized layer.

**A21.** Critical frequency.

**A22.** (a) density of ionization of the layer (b) frequency (c) angle at which it enters the layer

**A23.** A zone of silence between the ground wave and sky wave where there is no reception.

**A24.** Where ionization density is greatest.

**A25.** A term used to describe the multiple pattern a radio wave may follow.

**A26.** Selective fading.

**A27.** Natural and man-made interference.

**A28.** Natural.

**A29.** Man-made.

**A30.** (a) filtering and shielding of the transmitter (b) limiting bandwidth (c) cutting the antenna to the correct frequency

**A31.** (a) physical separation of the antenna (b) limiting bandwidth of the antenna (c) use of directional antennas

**A32.** Regular and irregular variations.

**A33.** Regular variations can be predicted but irregular variations are unpredictable.

**A34.** Daily, seasonal, 11-year, and 27-days variation.

**A35.** Sporadic E, sudden disturbances, and ionospheric storms.

**A36.** Muf is maximum usable frequency. Luf is lowest usable frequency. Fot is commonly known as optimum working frequency.

**A37.** Muf is highest around noon. Ultraviolet light waves from the sun are most intense.

**A38.** When luf is too low it is absorbed and is too weak for reception.

**A39.** Signal-to-noise ratio is low and the probability of multipath propagation is greater.

**A40.** Frequent signal fading and dropouts.

**A41.** Fot is the most practical operating frequency that can be relied on to avoid problems of multipath, absorbtion, and noise.

**A42.** They can cause attenuation by scattering.

**A43.** It can cause attenuation by absorbtion.

**A44.** It is a condition where layers of warm air are formed above layers of cool air.

**A45.** It can cause vhf and uhf transmission to be propagated far beyond normal line-of-sight distances.

**A46.** Troposphere.

**A47.** Vhf and above.

**A48.** Near the mid-point between the transmitting and receiving antennas, just above the radio horizon.
