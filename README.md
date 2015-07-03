# !!!! PLEASE READ THIS BEFORE DOWNLOADING ANY FILES !!!!

There have been a number of misunderstandings about the DPS8 GIT source code tree; sometimes leading to unfortunate user frustration. 

The canonical usage of GIT generally has the master branch containing "releases"; they are expected to be "ready to go". Development takes place on branches; once the development branch is done, it is moved to the master branch as a release. The DPS8 emulator project is not following that model. With just a handful of developers, and any "release" being a hoped-for dream in the unimaginable future, the master branch is the bleeding-edge development code. Commit points on the master branch probably compile on the machine of the person making the commit, but there is no promise of functionality, portability, correctness, update procedures, testing or even that it will compile.

The Alpha 1.0 release was done as a tag on the master branch; this was poor planning on our part as it made it difficult to apply bug fixes to generate a 1.1 version. The Alpha 2.0 release is being done as a branch; this will allow bug fixes to be merged in from the master branch, enabling the creation of release candidates, and future 2.x releases.

As with the master branch please realize that there is no guarantee or implied understanding that release candidates are to work or able to perform any useful function. In this way you, our user community, will not by under any false assumptions about the quality of our release candidate distributions.

The DPS8M team would like to apologize for any difficulties our unorthodox and informal policies may have incurred.



# SIMH v4.0 - Beta

## WHAT'S NEW

### New FNP Simulator

dps8/FNP - Faux front-end Network Processor (FNP) for the dps8m Multics emulator

NOTE: 
    1) At ths time simh/FNP requires the latest Code Connected ZeroMQ \zero-em-queue\, \ØMQ\ messaging libraries zmq, czmq & zyre
    See  <http://www.zeromq.org/> and  <https://github.com/zeromq>
	2) At this time simh/fnp provides no functional device connections to the Multics emulator. So, don't expect to login to Multics just yet as simh/dps8m is just starting to incorporate IPC mechanisms preparing for the FNP emulation. Sorry :-(


The following special simh/fnp specific commands are currently supported:

To run the zyre self-test:
    show IPC test

To enable/disable IPC operations:
    set IPC enable/disable

To start/stop the IPC service:
    set IPC start/stop

To set/show ipc node (default is 'fnp')
    set IPC node=fnp    
    (NOTE: when the node name is changed from 'fnp' to something else the sim prompt will become "NewNodeName>" to remind the user that the node name has changed. Changing the node name will allow multiple instances of sim/fnp to function as different FNPs)
    show IPC node
    
To set/show IPC group (default is MulticsIPC):
    show IPC group
    set IPC group=MulticsIPC

To set/show all IPC registered peers:
    show IPC peers

To remove all registered peers:
    set IPC removepeers

To attach telnet multiplexor to <port>:
    attach MUX -am 12345

To send a Broadcast message to all members (peers) of IPC group
    shout "Broadcase message"

To send a per-to-peer message:
    whisper <PEER-NAME>,"<Message>" -or- <PEER-GUID>,"<Message>"
or to send a message to the same peer as specified by the last 'whisper' commend.
    whisper "<Message>"    

To load device information:
    load [-VA] Devices.txt   
    
    (NOTE: Specifying the -V option selects the verbose mode. While the -A option appends the specified device list to the existing device list.)

After the user has entered the 'g' mode (by entering 'g' at the sim> prompt) the followoing single letter connands will take effect:
    'd' = disconnect all telnet connections
    's' = show summary line statistics on all telnet connections
    't' = persorm zyre self-test (same as 'show IPC test')
    'q' = quit. Return to the sim> prompt


sim> set IPC enable
sim> set IPC start
sim> load -V Devices.txt
sim> attach MUX -am 12345   <== or whatever port number you desire. 
sim> g

(At this point the user can telnet into simh/fnp via the *NIX shell command 'telnet localhost 12345') and receive the following prompt:

    Trying ::1...
    Connected to localhost.
    Escape character is '^]'.

    Connected to the FNP (Multics Faux FNP) simulator MUX device, line 0

    HSLA Port (d.h002,d.h006,d.h012,d.h013,d.h014,d.h015,d.h022,d.h023)? 


The user should then enter one of the Multics Faux devices as listed as shown below:

    HSLA Port (d.h002,d.h006,d.h012,d.h013,d.h014,d.h015,d.h022,d.h023)? d.h002
    Line 0 connected as
    name:            d.h002
    baud:            auto
    comment:         "cable 12319, X-6400, Vadic VA3467."

(To terminate the telnet connection the user can enter either the <ESC> or ^C characters to return the user to the *NIX command prompt._)
After a telnet session with simh/fnp is established the telnet input is echoed back to the telnet client.

N.B.: While executing in the 'g' mode a variety of messages will appear indicating connection status, etc. To return to the sim> prompt enter a single 'q.'

After returning back to the sim> prompt, the IPC service should be stopped and the MUX detatched by entering the following commands at the sim> prompt

sim> set IPC stop
sim> detach MUX
sim> q


###And now for 'watcher'

Watcher is derived from the zyre chat program and allows for limited eavesdropping on the IPC between the fnp and MulticsCS nodes. To execute watcher from a *NIX shell just emter "./watcher -v newatcher" and the IPC node 'newatcher' will start up listening for IPC traffic with all sorts of information about detected peers and messages being displayed.

You can send or receive broadcast or peer-to-peer messages via watcher. 

To send a broadcast message to all connected peers just prepend a ! to the input line thusly...

    !this will be a broadcast message.

And the input line will be broadcast to all connected peers.

To send a peer-to-peer message just type an input line (terminated by \n) and the message will be send to the last peer 'watcher' found.

    this is a peer-to-peer message

You can run two (or more) watcher instances and have them talk to each other - just specify differing nodeNames - e.g. w1 & w2.

If you start up a watcher the start up simh/fnp you'll see both fnp and watcher exchange IPC traffic.

To exit watcher just use enter your interrupt character - typically ^C



### New Host Platform support - HP-UX and AIX

### New Functionality

#### Remote Console Facility
A new capability has been added which allows a TELNET Connection to a user designated port so that some out of band commands can be entered to manipulate and/or adjust a running simulator.  The commands which enable and control this capability are SET REMOTE TELNET=port, SET REMOTE CONNECTIONS=n, SET REMOTE TIMEOUT=seconds, and SHOW REMOTE.

The remote console facility has two modes of operation: 1) single command mode. and 2) multiple command mode.  
In single command mode you enter one command at a time and aren't concerned about what the simulated system is doing while you enter that command.  The command is executed once you've hit return.
In multiple command mode you initiate your activities by entering the WRU character (usually ^E).  This will suspend the current simulator execution.  You then enter commands as needed and when you are done you enter a CONTINUE command.  While entering Multiple Command commands, if you fail to enter a complete command before the timeout (specified by "SET REMOTE TIMEOUT=seconds"), a CONTINUE command is automatically processed and simulation proceeds.

A subset of normal simh commands are available for use in remote console sessions.
The Single Command Mode commands are: ATTACH, DETACH, PWD, SHOW, DIR, LS, ECHO, HELP
The Multiple Command Mode commands are: EXAMINE, IEXAMINE, DEPOSIT, EVALUATE, ATTACH, DETACH, ASSIGN, DEASSIGN, STEP, CONTINUE, PWD, SAVE, SET, SHOW, DIR, LS, ECHO, HELP

A remote console session will close when an EOF character is entered (i.e. ^D or ^Z).

#### Terminal Multiplexer additions
    Added support for TCP connections using IPv4 and/or IPv6.
    Logging - Traffic going out individual lines can be optionally logged to 
            files
    Buffering - Traffic going to a multiplexor (or Console) line can 
            optionally be buffered while a telnet session is not connected
            and the buffered contents will be sent out a newly connecting 
            telnet session.  This allows a user to review what may have 
            happened before they connect to that session.

    Serial Port support based on work by J David Bryan and Holger Veit
    Serial Console Support
    Separate TCP listening ports per line
    Outgoing connections per line (virtual Null Modem cable).
    Packet sending and reception semantics for simulated network device support using either TCP or UDP transport.

#### Asynchronous I/O
    * Disk and Tape I/O can be asynchronous.  Asynchronous support exists 
      for pdp11_rq, pdp11_rp and pdp11_tq devices (used by VAX and PDP11 
      simulators).
    * Multiplexer I/O (Telnet and/or Serial) can be asynchronous.  
      Asynchronous support exists for console I/O and most multiplexer 
      devices.  (Still experimental - not currently by default)

#### Disk Extensions
    RAW Disk Access (including CDROM)
    Virtual Disk Container files, including differincing disks

#### Embedded ROM support
    Simulators which have boot commands which load constant files as part of 
    booting have those files imbedded into the simulator executable.  The 
    imbedded files are used if the normal boot file isn't found when the 
    simulator boots.  Specific examples are:  VAX (MicroVAX 3900 - ka655x.bin), 
    VAX8600 (VAX 8600 - vmb.exe), VAX780 (VAX 11/780 - vmb.exe), 
    VAX750 (VAX 11/750 - vmb.exe), VAX730 (VAX 11/730 - vmb.exe), 
    VAX610 (MicroVAX I - ka610.bin), VAX620 (rtVAX 1000 - ka620.bin), 
    VAX630 (MicroVAX II - ka630.bin)

#### Control Flow

The following extensions to the SCP command language without affecting prior behavior:

    GOTO <Label>                 Command is now available.  Labels are lines 
                                 in which the first non whitespace character 
                                 is a ":".  The target of a goto is the first 
                                 matching label in the current do command 
                                 file which is encountered.  Since labels 
                                 don't do anything else besides being the 
                                 targets of goto's, they could be used to 
                                 provide comments in do command files, for 
                                 example (":: This is a comment")
    SET ON                       Enables error trapping for currently defined 
                                 traps (by ON commands)
    SET NOON                     Disables error trapping for currently 
                                 defined traps (by ON commands)
    RETURN                       Return from the current do command file 
                                 execution with the status from the last 
                                 executed command
    RETURN <statusvalue>         Return from the current do command file 
                                 execution with the indicated status.  Status 
                                 can be a number or a SCPE_<conditionname> 
                                 name string.
    ON <statusvalue> commandtoprocess{; additionalcommandtoprocess}
                                 Sets the action(s) to take when the specific 
                                 error status is returned by a command in the 
                                 currently running do command file.  Multiple 
                                 actions can be specified with each delimited 
                                 by a semicolon character (just like 
                                 breakpoint action commands).
    ON ERROR commandtoprocess{; additionalcommandtoprocess}
                                 Sets the default action(s) to take when any 
                                 otherwise unspecified error status is returned 
                                 by a command in the currently running do 
                                 command file.  Multiple actions can be 
                                 specified with each delimited by a semicolon 
                                 character (just like breakpoint action 
                                 commands).
    ON <statusvalue>                   
    ON ERROR                     Clears the default actions to take when any 
                                 otherwise unspecified error status is 
                                 returned by a command in the currently 
                                 running do command file.


Error traps can be taken for any command which returns a status other than SCPE_STEP, SCPE_OK, and SCPE_EXIT.   

ON Traps can specify any status value from the following list: NXM, UNATT, IOERR, CSUM, FMT, NOATT, OPENERR, MEM, ARG, STEP, UNK, RO, INCOMP, STOP, TTIERR, TTOERR, EOF, REL, NOPARAM, ALATT, TIMER, SIGERR, TTYERR, SUB, NOFNC, UDIS, NORO, INVSW, MISVAL, 2FARG, 2MARG, NXDEV, NXUN, NXREG, NXPAR, NEST, IERR, MTRLNT, LOST, TTMO, STALL, AFAIL.  These values can be indicated by name or by their internal numeric value (not recommended).

Interactions with ASSERT command and "DO -e":
DO -e		is equivalent to SET ON, which by itself it equivalent to "SET ON; ON ERROR RETURN".
ASSERT		failure have several different actions:
       If error trapping is not enabled then AFAIL causes exit from the current do command file.
       If error trapping is enabled and an explicit "ON AFAIL" action is defined, then the specified action is performed.
       If error trapping is enabled and no "ON AFAIL" action is defined, then an AFAIL causes exit from the current do command file.

Other related changes/extensions:
The "!" command (execute a command on the local OS), now returns the command's exit status as the status from the "!" command.  This allows ON conditions to handle error status responses from OS commands and act as desired.

#### Scriptable interactions with running simulators.

The EXPECT command now exists to provide a means of reacting to simulator output and the SEND command exists to inject data into programs running within a simulator.

    EXPECT {HALTAFTER=n,}"\r\nPassword: "
    SEND {AFTER=n,}{DELAY=m,}"mypassword\r"
    
    or
    
    EXPECT {HALTAFTER=n,}"\r\nPassword: " SEND {AFTER=n,}{DELAY=m,}"mypassword\r"; GO
    

#### Help

    HELP dev
    HELP dev ATTACH
    HELP dev SET  (aka HELP SET dev)
    HELP dev SHOW (aka HELP SHOW dev)
    HELP dev REGISTERS

#### Generic scp support Clock Coscheduling as opposed to per simulator implementations.

#### New SCP Commands:

    SET ENVIRONMENT Name=Value      Set Environment variable
    SET ASYNCH                      Enable Asynchronous I/O
    SET NOASYNCH                    Disable Asynchronous I/O
    SET VERIFY                      Enable commang display while processing DO command files
    SET NOVERIFY                    Enable commang display while processing DO command files
    SET MESSAGE                     Enable error message output when commands complete (default)
    SET NOMESSAGE                   Disable error message output when commands complete
    SET QUIET                       Set minimal output mode for command execution
    SET NOQUIET                     Set normal output mode for command execution
    SET PROMPT                      Change the prompt used by the simulator (defaulr sim>)
    SET THROTTLE x/t                Throttle t ms every x cycles
    SET REMOTE TELNET=port          Specify remote console telnet port
    SET REMOTE NOTELNET             Disables remote console
    SET REMOTE CONNECTIONS=n        Specify the number of concurrent remote console sessions
    SHOW FEATURES                   Displays the devices descriptions and features
    SHOW ASYNCH                     Display the current Asynchronous I/O status
    SHOW SERIAL                     Display the available and/or open serial ports
    SHOW ETHERNET                   Display the available and/or open ethernet connections
    SHOW MULTIPLEXER                Display the details about open multiplexer devices
    SHOW CLOCKS                     Display the details about calibrated timers
    SHOW REMOTE                     Display the remote console configuration
    SHOW ON                         Display ON condition dispatch actions
    SET ON                          Enable ON condition error dispatching
    SET NOON                        Disable ON condition error dispatching
    GOTO                            Transfer to lable in the current DO command file
    CALL                            Call subroutine at indicated label
    RETURN                          Return from subroutine call
    SHIFT                           Slide argument parameters %1 thru %9 left 1
    NOOP                            A no-op command
    ON                              Establish or cancel an ON condition dispatch
    IF                              Test some simulator state and conditionally execute commands
    CD                              Change working directory
    SET DEFAULT                     Change working directory
    PWD                             Show working directory
    SHOW DEFAULT                    Show working directory
    DIR {path|file}                 Display file listing
    LS {path|file}                  Display file listing
    NEXT                            Step across a subroutine call or step a single instruction.
    EXPECT                          React to output produced by a simulated system
    SEND                            Inject input to a simulated system's console

#### Command Processing Enhancements

##### Environment variable insertion
Built In variables %DATE%, %TIME%, %DATETIME%, %LDATE%, %LTIME%, %CTIME%, %DATE_YYYY%, %DATE_YY%, %DATE_MM%, %DATE_DD%, %DATE_D%, %DATE_WYYYY%, %DATE_WW%, %TIME_HH%, %TIME_MM%, %TIME_SS%, %STATUS%, %TSTATUS%, %SIM_VERIFY%, %SIM_QUIET%, %SIM_MESSAGE%
Command Aliases

   Token "%0" expands to the command file name. 
   Token %n (n being a single digit) expands to the n'th argument
   Tonen %* expands to the whole set of arguments (%1 ... %9)

   The input sequence "\%" represents a literal "%", and "\\" represents a
   literal "\".  All other character combinations are rendered literally.

   Omitted parameters result in null-string substitutions.

   A Tokens preceeded and followed by % characters are expanded as environment
   variables, and if one isn't found then can be one of several special 
   variables: 
   
          %DATE%              yyyy-mm-dd
          %TIME%              hh:mm:ss
          %DATETIME%          yyyy-mm-ddThh:mm:ss
          %LDATE%             mm/dd/yy (Locale Formatted)
          %LTIME%             hh:mm:ss am/pm (Locale Formatted)
          %CTIME%             Www Mmm dd hh:mm:ss yyyy (Locale Formatted)
          %DATE_YYYY%         yyyy        (0000-9999)
          %DATE_YY%           yy          (00-99)
          %DATE_MM%           mm          (01-12)
          %DATE_DD%           dd          (01-31)
          %DATE_WW%           ww          (01-53)     ISO 8601 week number
          %DATE_WYYYY%        yyyy        (0000-9999) ISO 8601 week year number
          %DATE_D%            d           (1-7)       ISO 8601 day of week
          %DATE_JJJ%          jjj         (001-366) day of year
          %TIME_HH%           hh          (00-23)
          %TIME_MM%           mm          (00-59)
          %TIME_SS%           ss          (00-59)
          %STATUS%            Status value from the last command executed
          %TSTATUS%           The text form of the last status value
          %SIM_VERIFY%        The Verify/Verbose mode of the current Do command file
          %SIM_VERBOSE%       The Verify/Verbose mode of the current Do command file
          %SIM_QUIET%         The Quiet mode of the current Do command file
          %SIM_MESSAGE%       The message display status of the current Do command file
          
   Environment variable lookups are done first with the precise name between 
   the % characters and if that fails, then the name between the % characters
   is upcased and a lookup of that valus is attempted.

   The first Space delimited token on the line is extracted in uppercase and 
   then looked up as an environment variable.  If found it the value is 
   supstituted for the original string before expanding everything else.  If 
   it is not found, then the original beginning token on the line is left 
   untouched.

##### Command aliases
   commands can be aliases with environment variables.  For example:
   
      sim> set env say=echo
      sim> say Hello there
      Hello there

##### Do command argument manipulation

The SHIFT command will shift the %1 thru %9 arguments to the left one position.

## Building and running a simulator

### Use Prebuilt Windows Simulators

Simulators for the Windows platform are built and made available on a regular basis (at least once a week if changes have been made to the codebase).  

The prebuilt Windows binaries will run on all versions of Microsoft Windows from Windows XP onward.

They can be accessed at https://github.com/simh/Win32-Development-Binaries

Several relatively recent versions should be available which you can download and use directly.

### Building simulators yourself

First download the latest source code from the github repository's master branch at https://github.com/simh/simh/archive/master.zip

Depending on your host platform one of the following steps should be followed:

#### Linux/OSX other *nix platforms

If you are interested in using a simulator with Ethernet networking support (i.e. one of the VAX simulators or the PDP11), then you should make sure you have the correct networking components available.  The instructions in https://github.com/simh/simh/blob/master/0readme_ethernet.txt describe the required steps to get ethernet networking components installed and how to configure your environment.

See the 0readme_ethernet.txt file for details about the required network components for your platform.  Once your operating system has the correct networking components available the following command will build working simulators:

   $ make {simulator-name (i.e. vax)}

#### Windows

Compiling on windows is supported with recent versions of Microsoft Visual Studio (Standard or Express) and using GCC via the MinGW environment.  Things may also work under Cygwin, but that is not the preferred windows environment.  Not all features will be available as well as with either Visual Studio or MinGW.

##### Required related files.  The file https://github.com/simh/simh/blob/master/Visual%20Studio%20Projects/0ReadMe_Projects.txt

##### Visual Studio (Standard or Express) 2008, 2010 or 2012

The file https://github.com/simh/simh/blob/master/Visual%20Studio%20Projects/0ReadMe_Projects.txt describes the required steps to use the setup your environment to build using Visual Studio.

##### MinGW

The file https://github.com/simh/simh/blob/master/Visual%20Studio%20Projects/0ReadMe_Projects.txt describes the required steps to use the setup your environment to build using MinGW.

#### VMS

Download the latest source code as a zip file from: https://github.com/simh/simh/archive/master.zip

Unzip it in the directory that you want SIMH to reside in.  Unpack it and 
set the file attributes as follows:

    $ unzip simh-master.zip
    $ set default [.simh-master]
    $ set file/attri=RFM:STM makefile,*.mms,[...]*.c,[...]*.h,[...]*.txt

Simulators with ethernet network devices (All the VAX simulators and the 
PDP11) can have functioning networking when running on Alpha or IA64 OpenVMS.

In order to build and run simulators with networking support, the VMS-PCAP 
package must be available while building your simulator.  The simh-vms-pcap.zip 
file can be downloaded from https://github.com/simh/simh/archive/vms-pcap.zip   
This link will return a file called simh-vms-pcap.zip which should be unpacked as follows:

    $ unzip -aa simh-vms-pcap.zip
    $ rename [.simh-vms-pcap]pcap-vms.dir []

The PCAP-VMS components are presumed (by the descript.mms file) to be 
located in a directory at the same level as the directory containing the 
simh source files.  For example, if these exist here:

[]descrip.mms
[]scp.c
etc.

Then the following should exist: 
[-.PCAP-VMS]BUILD_ALL.COM
[-.PCAP-VMS.PCAP-VCI]
[-.PCAP-VMS.PCAPVCM]
etc.

To build simulators:

On a VAX use:

    $ MMx

On a Alpha & IA64 hosts use:

    $ MMx                        ! With Ethernet support
    $ MMx/MACRO=(NONETWORK=1)    ! Without Ethernet support

UNZIP can be found on the VMS freeware CDs, or from www.info-zip.org
MMS (Module Management System) can be licensed from HP/Compaq/Digital as part of the VMS Hobbyist program (it is a component of the DECSET product).
MMK can be found on the VMS freeware CDs, or from http://www.kednos.com/kednos/Open_Source/MMK
DEC C can be licensed from HP/Compaq/Digital as part of the VMS Hobbyist program.

## Problem Reports

If you find problems or have suggestions relating to any simulator or the simh package as a whole, please report these using the github "Issue" interface at https://github.com/simh/simh/issues.

Problem reports should contain;
 - a description of the problem
 - the simulator you experience the problem with
 - your host platform (and OS version)
 - how you built the simulator or that you're using prebuilt binaries
 - the simulator build description should include the output produced by while building the simulator
 - the output of SHOW VERSION while running the simulator which is having an issue
 - the simulator configuration file (or commands) which were used when the problem occurred.
 
