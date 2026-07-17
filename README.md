# Ass Effect

**Ass Effect**, creado por **Wamphyre**, es un efecto VST3 exclusivo para Linux de carácter lo-fi para pistas individuales y buses/master. Modela cassette, vinilo y equipo barato o dañado sin obligarte a manejar una cadena de diez plugins.

## Máquinas

- **90s Cassette**: compresión/saturación magnética con memoria, head bump, hiss, pérdida de agudos, wow/flutter y dropouts.
- **Worn Vinyl**: ancho de banda de una reproducción gastada, rumble, ruido de superficie, polvo, clicks y wow lento.
- **4-Track Demo**: cinta estrecha más saturada, oscura e inestable, pensada para guitarras y demos crudas.
- **Cellar Speaker**: altavoz pequeño, band-pass, recorte asimétrico y reducción agresiva para voces, batería o reamping paralelo.
- **Bitrot Sampler**: sample-hold y reducción de resolución para una degradación digital fría.

El ruido y las averías se generan dentro del DSP: no se repiten samples de ruido ni hacen falta archivos externos en tiempo de ejecución.

## Controles

| Control | Función |
| --- | --- |
| Drive | Nivel que entra en la saturación de la máquina. |
| Age | Pérdida de banda, lentitud magnética y envejecimiento general. |
| Wear | Dropouts en cinta o densidad de polvo/clicks en vinilo. |
| Wow / Flutter | Inestabilidad lenta y rápida de velocidad. |
| Noise | Hiss de cinta o ruido de superficie/rumble de vinilo. |
| Grit | Recorte, sample-hold y reducción de bits dependiente de la máquina. |
| Tone | Compensa o exagera la oscuridad de la máquina. |
| Width | Imagen estéreo de mono a 150 %. |
| Mix | Procesamiento paralelo dry/wet. |
| Output | Ganancia final. |

Todos los controles, incluida la selección de máquina y el bypass, son parámetros automatizables y se guardan con la sesión del DAW.

## Presets incluidos

`90s Walkman`, `Worn Pressing`, `4-Track Guitar`, `Buried Bass`, `Cellar Drums`, `Crypt Vocals`, `Rehearsal Mic`, `Frozen Sampler`, `Necro Master`, `Raw Master` y `Dust Dub`.

Los presets de master usan menos mezcla y degradación. Para master conviene igualar el volumen con **Output** antes de comparar; para un efecto extremo en pista, sube **Mix** al 100 %.

## Compilar y empaquetar

En Linux se necesitan CMake, Git, un compilador C++17 y las cabeceras de X11/Freetype/Fontconfig que usa la GUI de JUCE. En Debian/Ubuntu:

```bash
sudo apt install build-essential cmake git pkg-config \
  libfreetype6-dev libfontconfig1-dev libx11-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxcursor-dev
```

Después:

```bash
./build.sh
```

El script:

1. comprueba herramientas y dependencias;
2. descarga JUCE 8.0.10 una sola vez en `JUCE/`;
3. configura y compila en Release;
4. copia el bundle a `releases/Ass Effect.vst3` y genera su SHA-256;
5. elimina `build-release/` al terminar, también si hay un error.

JUCE se conserva como dependencia cacheada y `releases/` no se borra entero: solo se reemplaza el bundle de Ass Effect. Para conservar temporalmente el árbol de compilación al depurar:

```bash
KEEP_BUILD=1 JOBS=4 ./build.sh
```

Las optimizaciones específicas de la CPU están desactivadas para producir un binario portable. Pueden activarse con `NATIVE_OPTIMIZATIONS=ON`.

### Instalar en Linux

```bash
mkdir -p ~/.vst3
cp -a "releases/Ass Effect.vst3" ~/.vst3/
```

Reinicia o vuelve a escanear plugins en el DAW.

## Estructura

- `Source/LoFiEngine.*`: DSP en tiempo real sin asignaciones de memoria durante el procesado.
- `Source/PluginProcessor.*`: parámetros, presets, estado y entrada/salida del plugin.
- `Source/PluginEditor.*`: interfaz redimensionable y medidores.
- `assets/ass-effect-logo.svg`: logo vectorial editable.
- `build.sh`: build reproducible, release y limpieza.

## Formato y compatibilidad

- Plataforma: exclusivamente Linux; la release incluida es para x86_64.
- Autor y fabricante: Wamphyre.
- Formato: VST3, efecto mono o estéreo.
- Framework: JUCE 8.0.10.
- C++17 y CMake 3.22 o superior.
- Sin telemetría, navegador embebido ni dependencia de red en el plugin compilado.
