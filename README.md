# dab-cmdline for use with FMLIST Scanner 

This is a fork of https://github.com/JvanKatwijk/dab-cmdline/ especially for use with https://github.com/hayguen/fmlist_scan 

Compared to the initial project, it only contains basic inputs (rtlsdr, rtl_tcp, rawfiles, wavefile) as the FMLIST scan is based on RTLSDR sticks only.

## Example-10

### Command line options

The `example-10` binary prints the following command line options via `printOptions()`.
Some options are only available depending on the build configuration and selected input backend.

- `-W number`: amount of time to look for a time sync in ms
- `-A number`: amount of time to look for an ensemble in ms
- `-E minSNR`: activate scan mode; quit after loading scan data, and also quit if SNR is below `minSNR`
- `-t number`: determine TII every number of frames, default `10`
- `-a alfa`: update TII spectral power with factor `alfa` in range `0..1`, default `0.9`
- `-r number`: reset TII spectral power every number of frames, default `10`
- `-x`: switch TII algorithm to the extended one
- `-D`: enable detailed audio analysis in scan output
- `-c`: activate CSV output mode
- `-M mode`: DAB mode `1`, `2` or `4`, default `1`
- `-B band`: band is `L_BAND` or `BAND_III` (default)
- `-P name`: select the program by name
- `-p ppmCorr`: set ppm correction
- `-T tol`: tolerate mismatch of up to `tol` ms from system time before recording is aborted; default `-1`, negative values disable aborting
- `-S hexnumber`: use hexadecimal service id to identify a program
- `-w fileName`: write audio to a wave file
- `-f`: write binary FIC data to `ficdata.fic`
- `-n runtime`: stream or save audio for `runtime` seconds, then exit
- `-z freq_off`: set initial coarse frequency offset
- `-v`: increase verbosity

Options available for file input builds (`HAVE_WAVFILES` or `HAVE_RAWFILES`):

- `-F filename`: use file input
- `-o offset`: start file playback at offset in seconds
- `-R`: disable repetition of file playback
- `-X`: disable realtime pacing for file playback (consume as fast as processing allows)
- `-Y`: rewind file to the analysis start point (fallback: beginning) before each detailed audio analysis probe (`-D`)

Options available for non-file input builds:

- `-C channel`: select the DAB channel
- `-G gain`: set device gain, range `1..100`
- `-Q`: enable device autogain

Options available for RTL-SDR builds (`HAVE_RTLSDR`):

- `-d index`: set RTL-SDR device index
- `-s serial`: set RTL-SDR device serial

Options available for `rtl_tcp` builds (`HAVE_RTL_TCP`):

- `-H hostname`: set `rtl_tcp` host name
- `-I port`: set `rtl_tcp` port number

```
dab-rtlsdr -E -Q -C 7D
```
will print a summary after some seconds:

```
programnameHandler: 'EPG-SPI         ' / 'EPG-SPI' (SId E0500004) is part of the ensemble
FIG0Ext5: Short, SubChId 15, language 21
FIG0Ext5: Short, SubChId 9, language 21
FIG0Ext5: Short, SubChId 6, language 21
FIG0Ext5: Short, SubChId 2, language 21
FIG0Ext5: Short, SubChId 17, language 21
programnameHandler: 'Radio TV Serie A' / 'Serie A' (SId 5228) is part of the ensemble
programnameHandler: 'EPG-SPI         ' / 'EPG-SPI' (SId E0500004) is part of the ensemble
programnameHandler: 'M DUE O DANCE   ' / 'DANCE' (SId 5287) is part of the ensemble
programnameHandler: 'ACI Radio       ' / 'ACIRadio' (SId 5227) is part of the ensemble
programnameHandler: 'Radio Maria     ' / 'R Maria' (SId 51CC) is part of the ensemble
programnameHandler: 'M DUE O         ' / 'M DUE O' (SId 5233) is part of the ensemble
programnameHandler: 'Deejay 30 Songs ' / '30 Songs' (SId 5283) is part of the ensemble

 1214: ensemblenameHandler: 'DAB Italia      ' / 'DAB Ita' ensemble (EId 5009) is recognized

programnameHandler: 'Radio 24 +1     ' / 'R24 +1' (SId 5282) is part of the ensemble
programnameHandler: 'Radio 24        ' / 'Radio 24' (SId 5245) is part of the ensemble
programnameHandler: 'RMALB.IA        ' / 'RMALB.IA' (SId 52CC) is part of the ensemble
programnameHandler: '105 DAB         ' / '105 DAB' (SId 5286) is part of the ensemble
programnameHandler: 'Radio Radicale  ' / 'Radicale' (SId 5210) is part of the ensemble
programnameHandler: 'RR News         ' / 'RR News' (SId 5284) is part of the ensemble
programnameHandler: '* RDS *         ' / '* RDS *' (SId 5264) is part of the ensemble
programnameHandler: 'R101            ' / 'R101' (SId 5215) is part of the ensemble
programnameHandler: 'Deejay          ' / 'Deejay' (SId 5214) is part of the ensemble
programnameHandler: '* RDS * Relax   ' / 'Relax' (SId 5281) is part of the ensemble
FIG0Ext5: Short, SubChId 15, language 21
FIG0Ext5: Short, SubChId 9, language 21
```

and a summary in `csv` format for the automatic upload of the FMLIST Scanner.

Using parameter `-D` it will print detailled audio information, which can later be used on FMLIST (section URDS). If there is no audio at all, it will detect it as well. For weak signals, there will be a remark and the checks are skipped to avoid false values.

Please note that the parameter `-D` will massivly increase the scan duration.

```
28762: checked program 'RADIO SUNBEAT   ' with SId 5FE6
        audioData:
                subchId         = 12
                startAddr       = 372
                shortForm       = false
                protLevel       = 2: 'EEP 3-A'
                codeRate        = 2: '1/2'
                length          = 48
                bitRate         = 64
                ASCTy           = 63: 'DAB+'
                audioCodec      = 'DAB+/HE-AAC v1 Stereo 48kHz'
                country ECC E0, Id 5: 'Italy'
                language        = 21: 'Italian'
                programType     = 10: 'Pop Music'
LIST: SID 00005FE6 of type A has 1 audio and 0 packet components: 'RADIO SUNBEAT   ' / 'SUNBEAT'
```

```
all meta info of fib_processor:
services:
 0( 1): SID 00005215, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[4,], label 1 'R101            ' / 'R101'
 1( 2): SID 00005227, lang -1, PTy  3, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[10,], label 1 'ACI Radio       ' / 'ACIRadio'
 2( 3): SID 00005282, lang -1, PTy  1, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[17,], label 1 'Radio 24 +1     ' / 'R24 +1'
 3( 4): SID 000051CC, lang -1, PTy  7, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[1,], label 1 'Radio Maria     ' / 'R Maria'
 4( 5): SID 00005214, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[3,], label 1 'Deejay          ' / 'Deejay'
 5( 6): SID E0500004, lang -1, PTy -1, pNum -1, #SCIds 1 =[0,], #SCId 1 =[1600,], #SubChId 1 =[25,], label 1 'EPG-SPI         ' / 'EPG-SPI'
 6( 7): SID 000052CC, lang -1, PTy  7, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[13,], label 1 'RMALB.IA        ' / 'RMALB.IA'
 7( 8): SID 00005281, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[14,], label 1 '* RDS * Relax   ' / 'Relax'
 8( 9): SID 00005245, lang -1, PTy  1, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[7,], label 1 'Radio 24        ' / 'Radio 24'
 9(10): SID 00005228, lang -1, PTy  4, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[12,], label 1 'Radio TV Serie A' / 'Serie A'
10(11): SID 00005286, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[11,], label 1 '105 DAB         ' / '105 DAB'
11(12): SID 00005283, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[18,], label 1 'Deejay 30 Songs ' / '30 Songs'
12(13): SID 00005284, lang -1, PTy  1, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[19,], label 1 'RR News         ' / 'RR News'
13(14): SID 00005285, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[15,], label 1 'Capital Funky   ' / 'CapitalF'
14(15): SID 00005219, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[5,], label 1 'Capital         ' / 'Capital'
15(16): SID 00005233, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[6,], label 1 'M DUE O         ' / 'M DUE O'
16(17): SID 00005287, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[16,], label 1 'M DUE O DANCE   ' / 'DANCE'
17(18): SID 00005264, lang -1, PTy 10, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[8,], label 1 '* RDS *         ' / '* RDS *'
18(19): SID 0000410E, lang -1, PTy 20, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[9,], label 1 'Radio Vaticana  ' / 'Vaticana'
19(20): SID 00005210, lang -1, PTy  1, pNum -1, #SCIds 1 =[0,], #SCId 0 =[], #SubChId 1 =[2,], label 1 'Radio Radicale  ' / 'Radicale'
components:
 0( 1): P( 1) TMid 3, SID E0500004, componentNr  0, ASCTy -1, PS 1, subChID   25, SCId 1600, CA 0, appType  7, SCIds 0, madePub 1, '' / '', DSCTy 60, DG 0, addr   1
 1( 2): A( 1) TMid 0, SID 00005228, componentNr  0, ASCTy 63, PS 1, subChID   12, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
 2( 3): A( 2) TMid 0, SID 00005287, componentNr  0, ASCTy 63, PS 1, subChID   16, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
 3( 4): A( 3) TMid 0, SID 00005227, componentNr  0, ASCTy 63, PS 1, subChID   10, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
 4( 5): A( 4) TMid 0, SID 000051CC, componentNr  0, ASCTy 63, PS 1, subChID    1, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
 5( 6): A( 5) TMid 0, SID 00005233, componentNr  0, ASCTy 63, PS 1, subChID    6, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
 6( 7): A( 6) TMid 0, SID 00005283, componentNr  0, ASCTy 63, PS 1, subChID   18, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
 7( 8): A( 7) TMid 0, SID 00005282, componentNr  0, ASCTy 63, PS 1, subChID   17, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
 8( 9): A( 8) TMid 0, SID 00005245, componentNr  0, ASCTy 63, PS 1, subChID    7, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
 9(10): A( 9) TMid 0, SID 000052CC, componentNr  0, ASCTy 63, PS 1, subChID   13, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
10(11): A(10) TMid 0, SID 00005286, componentNr  0, ASCTy 63, PS 1, subChID   11, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
11(12): A(11) TMid 0, SID 00005210, componentNr  0, ASCTy 63, PS 1, subChID    2, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
12(13): A(12) TMid 0, SID 00005284, componentNr  0, ASCTy 63, PS 1, subChID   19, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
13(14): A(13) TMid 0, SID 00005264, componentNr  0, ASCTy 63, PS 1, subChID    8, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
14(15): A(14) TMid 0, SID 00005215, componentNr  0, ASCTy 63, PS 1, subChID    4, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
15(16): A(15) TMid 0, SID 00005214, componentNr  0, ASCTy 63, PS 1, subChID    3, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
16(17): A(16) TMid 0, SID 00005281, componentNr  0, ASCTy 63, PS 1, subChID   14, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
17(18): A(17) TMid 0, SID 00005285, componentNr  0, ASCTy 63, PS 1, subChID   15, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
18(19): A(18) TMid 0, SID 00005219, componentNr  0, ASCTy 63, PS 1, subChID    5, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
19(20): A(19) TMid 0, SID 0000410E, componentNr  0, ASCTy 63, PS 1, subChID    9, SCId -1, CA 255, appType  2, SCIds 0, madePub 0, '' / ''
subChannels:
 1( 1): subChID    1, startAddr   0, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
 2( 2): subChID    2, startAddr  36, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
 3( 3): subChID    3, startAddr  72, len  48, sz  48, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  64, lang 21, FEC -1
 4( 4): subChID    4, startAddr 120, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
 5( 5): subChID    5, startAddr 156, len  48, sz  48, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  64, lang 21, FEC -1
 6( 6): subChID    6, startAddr 204, len  48, sz  48, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  64, lang 21, FEC -1
 7( 7): subChID    7, startAddr 252, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
 8( 8): subChID    8, startAddr 288, len  48, sz  48, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  64, lang 21, FEC -1
 9( 9): subChID    9, startAddr 336, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
10(10): subChID   10, startAddr 372, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
11(11): subChID   11, startAddr 408, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
12(12): subChID   12, startAddr 444, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
13(13): subChID   13, startAddr 480, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 1, FEC -1
14(14): subChID   14, startAddr 516, len  24, sz  24, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  32, lang 21, FEC -1
15(15): subChID   15, startAddr 540, len  24, sz  24, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  32, lang 21, FEC -1
16(16): subChID   16, startAddr 564, len  24, sz  24, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  32, lang 21, FEC -1
17(17): subChID   17, startAddr 588, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
18(18): subChID   18, startAddr 624, len  24, sz  24, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  32, lang 21, FEC -1
19(19): subChID   19, startAddr 648, len  36, sz  36, long, idxOrCase 102, tabIdx -1, protLevel 2, bitRate  48, lang 21, FEC -1
25(20): subChID   25, startAddr 684, len  48, sz  48, long, idxOrCase 101, tabIdx -1, protLevel 1, bitRate  48, lang -1, FEC -1
```

## Building

The build entry point of this fork is [example-10/CMakeLists.txt](example-10/CMakeLists.txt). It uses CMake and defaults to a `Release` build if no build type is specified.

Required core dependencies from the CMake configuration are:

- FFTW3
- FAAD / libfaad
- zlib
- pthread

Additional libraries depend on the selected input backend, for example `librtlsdr` for `RTLSDR` or the matching `rtl_tcp` support.

### Configure and build

Build from inside the `example-10` directory with an out-of-source build:

```sh
cd example-10
mkdir -p build
cd build
cmake .. -DRTLSDR=ON
make
```

This produces one backend-specific binary name, depending on the enabled input:

- `dab-rtlsdr` for `-DRTLSDR=ON`
- `dab-rtl_tcp` for `-DRTL_TCP=ON`
- `dab-files` for `-DWAVFILES=ON`
- `dab-raw` for `-DRAWFILES=ON`

If no input backend is selected explicitly, the build system falls back to `RTLSDR`.

### Optional: AbracaDABra `libdabsdr` probe

For integration experiments, this repository can build a small probe binary linked
against AbracaDABra's prebuilt `libdabsdr`.

The required AbracaDABra SDK files are expected under platform-specific paths,
for example:

- `third_party/abracadabra/lib/linux_x86_64/`
- `third_party/abracadabra/lib/linux_aarch64/`
- `third_party/abracadabra/lib/windows_amd64/`
- `third_party/abracadabra/lib/darwin_x86_64/`
- `third_party/abracadabra/lib/darwin_arm64/`

Build the probe from `example-10/build` with:

```sh
cmake .. -DABRA_DABSDR_PROBE=ON
make abra-dabsdr-probe
```

Run `abra-dabsdr-probe` to verify that `libdabsdr` loads and initializes.

To compare detection on the same IQ recording, build the optional raw scanner:

```sh
cmake .. -DABRA_RAW=ON
make abra-raw
```

To build the live RTL-SDR variant with the same ABRA_* output format:

```sh
cmake .. -DABRA_RTLSDR=ON
make
```

This produces `abra-rtlsdr` as part of the default build when `ABRA_RTLSDR=ON`.
It does not auto-enable the legacy `dab-rtlsdr` fallback target.

Example run (8-bit unsigned raw IQ, interleaved I/Q):

```sh
./abra-raw --file ./sample.raw --channel 11C --seconds 20
```

If `--channel` is omitted, `abra-raw` tries to auto-detect the channel
from the input filename (for example `..._8B_...raw`).

Detailed audio analysis (dab-raw-like) can be enabled with:

```sh
./abra-raw --file ./sample.raw --audio-details --rewind --log ./abra-scan.log
```

Useful options for that mode:

- `--audio-details` print per-component audio details (`ABRA_AUDIO` lines)
- `--rewind` rewind file before probing each audio service
- `--rewind-offset <sec>` seek to offset before each probe (default `0`)
- `--probe-ms <ms>` probe duration per audio component (default `1800`)
- `--log <file>` mirror tool output into a log file

`abra-raw` also accepts common `dab-raw` compatible short options,
so existing scripts can switch with minimal changes:

- `-F -C -B -D -Y -o -X -S -c -E -n`
- plus compatibility no-op acceptance for: `-W -A -P -p -T -M -t -a -r -z -G -H -I -O -Q -R -x -v -f`

The tool prints one summary line:

- `ABRA_SCAN_RESULT channel=... sync=... serviceList=... services=... ensemble="..." ...`

and one line per detected service:

- `ABRA_SERVICE sid=... label="..."`

With `--audio-details`, it additionally prints:

- `ABRA_AUDIO sid=... scids=... ascty=... bitrate=... codec="..." ...`

When using CSV scan mode (`-c`) without selecting a single SID (`-S`),
the `CSV_AUDIO` codec field keeps dab-rtlsdr-compatible generic labels.
If you also pass `--emit-abra`, the tool still performs deep probing for
`ABRA_AUDIO` lines, so detailed codec metadata remains available there.

### Configure with `ccmake`

You can also configure interactively with `ccmake`:

```sh
cd example-10
mkdir -p build
cd build
ccmake ..
```

Inside `ccmake`, enable exactly the input backend you want, for example:

- `RTLSDR`
- `RTL_TCP`
- `WAVFILES`
- `RAWFILES`

Then press `c` to configure, adjust remaining options if needed, press `c` again until generation is available, and press `g` to generate the build files. After that, build normally:

```sh
make
```

For ABRA-only builds in `ccmake`, you can keep `RTLSDR`, `RTL_TCP`, `WAVFILES`, and `RAWFILES` set to `OFF` and enable one or more of:

- `ABRA_DABSDR_PROBE`
- `ABRA_RAW`
- `ABRA_RTLSDR`

### Install

To install the resulting binaries:

```sh
sudo make install
```

The default install prefix in the CMake file is `/usr/local` unless you override `CMAKE_INSTALL_PREFIX`.

## Todo-List:

### ideas

- [x] add audio details (real decoder-based codec detection)
- [x] use new command line parameter `-D` to enable the detailed audio analysis

### fixed bugs

- [x] ECC and Country code (thanks to Jan van Katwijk for the bugfix)
- [x] ECC even works when the service ECC is different from the ensemble
- [x] codec output uses decoded stream metadata (AAC flags / MP2 mode), not bitrate estimation


# Copyrights
	
Copyright (C)  2016, 2017, 2026
Jan van Katwijk (J.vanKatwijk@gmail.com)
Lazy Chair Programming

The dab-library software is made available under the GPL-2.0. The dab-library uses a number of GPL-ed libraries, all rights gratefully acknowledged. All SDR-J software, among which dab-library is one - is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

