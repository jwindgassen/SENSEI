SENSEI
======

This is the `Sensei` repository for the code for all the miniapps and analysis
routines.

Repository Organization
----------------------
    - CMake/
    
      This contains CMake files e.g. mpi.cmake, FindADIOS.cmake, etc.
      
    - sensei/
      
      This contains the source code for the all files under the 'sensei' namespace
      including the core components, DataAdaptor.[h,cxx], AnalysisAdaptor.[h,cxx],
      as well as example analysis adaptor implementations such as Histogram.[h,cxx],
      Autocorrelation.[h,cxx].
      
      All classes within this directory are under the 'sensei' namespace and application
      code can include these header using fully qualified paths e.g.
      
        #include <sensei/DataAdaptor.h> // for sensei::DataAdaptor declaration.
        #include <sensei/AnalysisAdaptor.h> // for sensei::AnalysisAdaptor declaration.
      
      Adaptor implementations that depend on insitu frameworks are contained within
      directories specific for that framework under this directory.
      
    - sensei/adios/
      
      Contains source code for all files under the 'sensei::adios' namespace.
      This houses sensei::DataAdaptor and sensei::AnalysisAdaptor implementations that read and write
      data using ADIOS APIs, namely, 'sensei::adios::DataAdaptor' and
      'sensei::adios::AnalysisAdaptor'.
        
    - sensei/catalyst/
    
      Contains source code for all files under the 'sensei::catalyst' namespace.
      This houses sensei::catalyst::AnalysisAdaptor which implements 'sensei::AnalysisAdaptor'
      interface to use Catalyst for the analysis. Since in Catalyst, analysis is performed
      via pipelines, this directory contains instances of pipelines (e.g. sensei::catalyst::Slice).

    - sensei/libsim

      Contains source code for all files under the 'sensei::libsim' namespace.
      This houses the Libsim analysis adaptor for making slice images, etc.
   
    - miniapps/
    
      All miniapps generated for various bechmarks and testing is included in this directory.
      Each miniapp includes both the expository implementation
      independent of Sensei infrastructure as well as a version that uses Sensei.
    
    - miniapps/parallel3d/
    
      Code for miniapp from year I. 
      
    - miniapps/oscillators/
    
      Core for miniapp from year II.
    
    - endpoints/
    
      Code for executables that act as analysis endpoints for frameworks like ADIOS (ADIOSAnalysisEndPoint.cxx)
      and GLEAN. These executables (will) use the sensei::adios::DataAdaptor and sensei::glean::DataAdaptor
      to ingest data from these frameworks back to Sensei for further analysis.
      
    - utils/
        - diy/
        - grid/
        - opts/
        - pugixml/
    
      Various utility code used by the various analysis implementations.
      
    - configs/
     
      Sample configuration XML files demonstrating how a configurable analysis pipeline
      can be setup.

Build instructions
---------------------

The project uses CMake 3.0 or later. The options provided allow you to choose
which of the miniapps to build as well as which frameworks to enable.

When **ENABLE_SENSEI** is off, none of the insitu frameworks are useable.
However, miniapps are trypically instrumented with a prototype analysis code
e.g. `3D_Grid` is set up to compute histograms while `oscillators` is set up to
do an autocorrelation analysis.

When **ENABLE_SENSEI**, the miniapps take in an configuration xml that is used
to configure analysis via Sensei infrastructure. Looks at the
[3dgrid.xml](configs/3dgrid.xml), [oscillator.xml](configs/oscillator.xml) and
[adiosendpoint.xml](configs/adiosendpoint.xml) for examples of these config
files.

Let's look at the various CMake flags and how they affect the generated build.

* **ENABLE_SENSEI**: (ON/OFF) Set to ON to enable `Sensei`.  Thus,
when **ENABLE_SENSEI** is OFF, miniapps will only support the one insitu analysis routine
they were hardcoded for. If ON, you will need to set the **VTK_DIR** to point to an existing VTK build since
`Sensei` depends on `vtkCommonDataModel` module for its data model and adaptor classes.

* **VTK_DIR**: Path to `VTKConfig.cmake` file. When building with **libsim** or **Catalyst**,
you should point this to the VTK build used/included by the two frameworks.

* **VTK_HAS_GENERIC_ARRAYS**: Set to ON if you have a custom VTK build with Generic Array support. The
current Sensei Histogram implementation uses generic arrays API and hence is not built unless
this is set to ON.

You can choose which miniapp to build using the following flags.

* **ENABLE_PARALLEL3D**: (ON/OFF) Set to ON to build the `parallel_3d` miniapp from miniapp campaign #1.
This miniapp can do histogram analysis if **ENABLE_SENSEI** is OFF. If **ENABLE_SENSEI** is ON, this will use the Sensei
bridge along with data and analysis adaptors to do the analysis specified in the configuration XML.

* **ENABLE_OSCILLATORS**: (ON/OFF) Set to ON to build the `oscillators` miniapp from miniapp campaign #2.
If **ENABLE_SENSEI** is OFF, this miniapp can do autocorrelation analysis alone. If **ENABLE_SENSEI** is ON, this miniapp supports the histogram,
autocorrelation, catalyst-slice as specified in a configuration xml.

To use analysis routines from Catalyst, you can use the following flags.

* **ENABLE_CATALYST**: (ON/OFF) Set to ON to enable analysis routines that use Catalyst. This option is
only available when **ENABLE_SENSEI** is ON. This builds an analysis adaptor for Sensei that invokes Catalyst calls
to do the data processing and visualization. When set to ON, you will have to point **ParaView_DIR** to a ParaView (or Catalyst) build
containing ParaViewConfig.cmake file.

* **ENABLE_ADIOS**: (ON/OFF) Set to ON to enable ADIOS components. When enabled,
this generates a **ADIOSAnalysisEndPoint** that can be used as an endpoint components
that reads data being serialized by the ADIOS analysis adaptor and pass it back
into a `Sensei` bridge for further analysis. **ADIOSAnalysisEndPoint** itself can be given
configuration XML for select analysis routines to run via Sensei infrastructure.

* **ENABLE_LIBSIM**: (ON/OFF) Set to ON to enable Libsim components. In addition, you must set LIBSIM_DIR so it points
to the location of the installed LIBSIMConfig.cmake file to locate Libsim and set up its dependencies. When these
settings are enabled/set, the Libsim analysis adaptor will be built.

Typical build usage:

    make build
    cd build
    ccmake .. # set cmake options as needed
    make
    
All executables will be generated under `bin`.

Miniapps/Executables
---------------------
Details on each of the miniapps are as follows:

* Parallel3D

        Usage (ENABLE_SENSEI=ON):
        ./bin/3D_Grid -g 4x4x4 -l 2x2x2 -f config.xml
            -g global dimensions
            -l local (per-process) dimensions
            -f Sensei xml configuration file for analysis

        Usage (ENABLE_SENSEI=OFF):
        ./bin/3D_Grid -g 4x4x4 -l 2x2x2 -b 10
            -g global dimensions
            -l local (per-process) dimensions
            -b histogram bins



* [Oscillators](miniapps/oscillators/README.md)

        Usage (ENABLE_SENSEI=ON):
        ./bin/oscillator [OPTIONS] OSCILLATORS.txt
        Options:
            -b, --blocks INT      number of blocks to use [default: 1]
            -s, --shape POINT     domain shape [default: 64 64 64]
            -t, --dt FLOAT        time step [default: 0.01]
            -f, --config STRING   Sensei analysis configuration xml (required)
            --t-end FLOAT         end time [default: 10]
            --sync                synchronize after each time step
           -h, --help             show help

        Usage (ENABLE_SENSEI=OFF):
        ./bin/oscillator [OPTIONS] OSCILLATORS.txt
        Options:
            -b, --blocks INT             number of blocks to use [default: 1]
            -s, --shape POINT            domain shape [default: 64 64 64]
            -t, --dt FLOAT               time step [default: 0.01]
            -w, --window UNSIGNED LONG   analysis window [default: 10]
            -k, --k-max UNSIGNED LONG    number of strongest autocorrelations to report [default: 3]
            --t-end FLOAT                end time [default: 10]
            --sync                       synchronize after each time step
            -h, --help                   show help

* ADIOSAnalysisEndPoint

        Usage (ENABLE_SENSEI=ON):
        ./bin/ADIOSAnalysisEndPoint[OPTIONS] input-stream-name
        Options:
           -r, --readmethod STRING   specify read method: bp, bp_aggregate, dataspaces, dimes, or flexpath  [default: bp]
           -f, --config STRING       Sensei analysis configuration xml (required)
           -h, --help                show help

        Usage (ENABLE_SENSEI=OFF):
        <NOT AVAILABLE>


