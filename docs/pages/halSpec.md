# LPA HAL Documentation

## Version History

| Date | Comment | Version |
| --- | --- | --- |
| 2024-05-24 | Initial release. LPA HAL interface header migrated to GitHub under RDKB-52499. | 1.0.0 |
| Not recorded | Maintenance release. Removed an unwanted character from the interface header. | 1.0.1 |
| 2026-08-24 | Specification rewritten against the interface header: the `eSIM` and `eUICC` subject matter corrected, the complete API surface documented, and the asynchronous notification, memory and error-handling contracts restated from the declarations. | Unreleased |

## Acronyms

- `HAL` \- Hardware Abstraction Layer
- `RDK-B` \- Reference Design Kit for Broadband Devices
- `OEM` \- Original Equipment Manufacturer
- `LPA` \- Local Profile Assistant
- `eSIM` \- Embedded Subscriber Identity Module
- `eUICC` \- Embedded Universal Integrated Circuit Card, the component an `eSIM` profile is installed onto
- `SIM` \- Subscriber Identity Module
- `UICC` \- Universal Integrated Circuit Card
- `USIM` \- Universal Subscriber Identity Module
- `EID` \- `eUICC` Identifier, the unique identifier of an `eUICC`
- `ICCID` \- Integrated Circuit Card Identification, the identifier of an individual profile
- `MNO` \- Mobile Network Operator
- `SM-DS` \- Subscription Manager Discovery Service
- `SM-DP+` \- Subscription Manager Data Preparation, the server that delivers a prepared profile
- `ITU-T` \- International Telecommunication Union, Telecommunication Standardization Sector

## Description

The `LPA` (Local Profile Assistant) HAL is the `RDK-B` interface for `eSIM` **profile management on an `eUICC`**. It abstracts a vendor's Local Profile Assistant implementation so that `RDK-B` software can download, activate, deactivate, remove and enumerate `MNO` profiles, and read the identity of the `eUICC` itself, without depending on a particular modem, `eUICC` or `OEM` implementation.

The diagram below shows where the LPA HAL sits in the software stack.

```mermaid
flowchart TD
    Caller["RDK-B Middleware"] <--> HAL["LPA HAL - lpa_hal.h"]
    HAL <--> Vendor["Vendor Software"]
    Vendor <--> Hardware["Modem and eUICC hardware"]
```

No dedicated `RDK-B` middleware service owns this interface. The caller, shown as RDK-B Middleware, is whichever `RDK-B` component needs `eSIM` management, and it links the vendor library directly.

The interface covers five areas:

- **Profile download:** installing a profile onto the `eUICC`, from an activation code, from an `SM-DS` address, or from a default `SM-DP+` address.
- **Profile lifecycle:** enabling, disabling and permanently deleting an installed profile, each selected by its `ICCID`.
- **Profile inspection:** enumerating the `MNO` profiles present on the `eUICC` with their `ICCID`, state and name.
- **Card identity:** reading the `EID` and the `eUICC` information.
- **Module lifecycle:** bringing the Local Profile Assistant up and tearing it down.

## Optional Components

This interface declares no optional build-time components: every declaration in `include/lpa_hal.h` is present in every build. Two forms of optionality are visible in the interface itself, and a caller plans for both:

- `cellular_esim_download_profile_with_activationcode()`, `cellular_esim_download_profile_from_smds()` and `cellular_esim_download_profile_from_defaultsmdp()` are **alternative** download paths, not a sequence. A deployment may use any one of them, and need not support all three.
- Download progress is reported on the activation-code path **only**; see `Asynchronous Notification Model`.

## Component Runtime Execution Requirements

The requirements in this block govern how an implementation behaves at run time and what a caller may rely on.

### Initialization and Startup

The Cellular `eSIM` `LPA` module requires no external API dependencies for initialization. `cellular_esim_lpa_init()` is **mandatory** and must be called before any other function in this interface.

- `cellular_esim_lpa_init()` \- brings the Local Profile Assistant up: configures the module, establishes modem readiness and sets up the dependencies and parameters the remaining operations need.
- `cellular_esim_lpa_exit()` \- deactivates the module and releases its resources. To be called before shutdown, or once `LPA` functionality is no longer needed.

Third-party vendors are responsible for providing a concrete implementation of `cellular_esim_lpa_init()`. That implementation must check that the necessary hardware — the `eUICC` and the modem — is present and functional, so that subsequent operations do not fail against missing or unprepared hardware.

### Threading Model

The interface is not required to be thread-safe. It is the responsibility of the calling module or component to ensure that all interactions with these APIs are properly synchronized.

Vendors may implement internal threading and event mechanisms to meet their operational requirements. Those mechanisms must ensure thread safety when interacting with this interface, and must guarantee cleanup of every resource they allocate — memory, file handles and threads — when the implementation terminates or the caller closes its use of the HAL.

A caller writes the body of `cellular_sim_download_progress_callback` to be safe on a thread it does not own, and serializes its own access to whatever that body touches.

### Process Model

All APIs are expected to be called from multiple processes. Because of this concurrent access, vendors must implement protection mechanisms within their implementations to handle multiple processes calling the same API simultaneously, so that data integrity is preserved, race conditions are prevented and the system remains stable.

### Memory Model

The caller owns every buffer it passes into this interface, and the only memory the interface hands back is the profile array returned by `cellular_esim_get_profile_info()`.

#### Caller Responsibilities

- **Free the profile array:** Release the array `cellular_esim_get_profile_info()` returns once that call has returned `RETURN_OK`, and before calling `cellular_esim_lpa_exit()`. After a failed call the output is undefined and is neither read nor freed.
- **Keep passed buffers valid:** Keep every buffer passed into a call allocated and unmodified during the call and afterwards, until the vendor confirms that the implementation has finished with it or `cellular_esim_lpa_exit()` has returned `RETURN_OK`. A tear-down that returns `RETURN_ERROR` may leave resources held, so the buffers stay allocated and unmodified after it until one of those conditions is met.
- **Zero-terminate every string:** All strings passed across this interface must be zero-terminated, so that string functions can determine their length and buffer manipulation cannot overrun.

#### Module Responsibilities

- Allocate and deallocate memory for internal operations, and release all internally allocated memory on closure, so that repeated bring-up and tear-down does not leak.
- Keep the profile array returned by `cellular_esim_get_profile_info()` valid until the caller releases it.

### Power Management Requirements

The LPA HAL is not involved in any power management operation. It neither participates in nor requires involvement in power state transitions, wake event configuration or resource throttling, and it declares no API for any of them.

### Asynchronous Notification Model

This interface provides **exactly one** asynchronous channel: `cellular_sim_download_progress_callback`, declared as `int (*)(char *progress)` and installed as the second argument of `cellular_esim_download_profile_with_activationcode()`.

- **Installed per call:** There is no registration or deregistration API. The callback's scope is the download it was passed to, so a caller cannot install a standing notification handler and has nothing to unregister.
- **Activation-code path only:** `cellular_esim_download_profile_from_smds()` and `cellular_esim_download_profile_from_defaultsmdp()` take no callback argument and report no progress to the caller.
- **The callback returns a status:** It returns `RETURN_OK` when it accepts a message and an error otherwise. A caller does not use an error return to cancel a download.
- **Delivery context:** See `Threading Model`.

The `progress` argument carries one of the following status messages:

- `Activation Code decoded`
- `Initiate authentication done`
- `SM-DP+ address checking done`
- `Authenticate server done`
- `Authenticate client done`
- `PPR conditions check done`
- `Get Bound Profile Package done`
- `Send PIR notification to server done`
- `Clear PIR notification done`
- `Read pending notifications...`
- `<1> pending notification(s) detected`
- `Processing notification #1 <33>`
- `Notification <33> successfully sent`
- `Load Bound Profile Package done - Profile download successful`
- `download successful`

Two of those messages embed values that vary at run time — the pending notification count and the notification identifier — so a caller matches them on a prefix. A caller treats the list as a vocabulary, not as an ordered sequence in which every message appears.

### Blocking calls

- **Synchronous and Responsive:** Every function returns its outcome directly as a status and is expected to complete within a time commensurate with the complexity of the operation requested.
- **Network-facing downloads:** The three download functions contact a remote `SM-DP+` or `SM-DS` server, so their duration includes a network round trip.
- **Caller-imposed bounds:** A caller makes each call where a delay is tolerable — `cellular_esim_lpa_init()` at bring-up and the three downloads in particular — and imposes and enforces its own bound where one is required.
- **Progress callback:** A callback body runs while the activation-code download is in progress, so it returns promptly and defers lengthy work to the caller's own context.

### Internal Error Handling

Every function returns an `int` status:

| Macro | Value | Meaning |
| --- | --- | --- |
| `RETURN_OK` | `0` | Success for ten of the eleven functions. The operation completed and its documented post-conditions hold. |
| `RETURN_ERROR` | `-1` | Failure for those ten functions. The caller relies on no output parameter and looks to the vendor log for the cause. |
| `TRUE` | `1` | Success for `cellular_esim_download_profile_with_activationcode()`. |
| `FALSE` | `0` | Failure for `cellular_esim_download_profile_with_activationcode()`. The caller treats no profile as installed until enumeration confirms otherwise. |

Caller code compares against `RETURN_ERROR`; the spelling `RETURN_ERR` is not defined by this interface and does not compile.

- **The activation-code download uses a different convention, and the collision is silent:** Because `FALSE` and `RETURN_OK` are both `0`, **a caller that tests the result of `cellular_esim_download_profile_with_activationcode()` against `RETURN_OK` reads a documented failure as a success**. A caller treats a non-zero result from that function as success only once it has confirmed the convention with its vendor, and does not reuse the comparison that is correct for the other ten functions.

### Persistence Model

There is no requirement for the HAL to persist any setting information. A caller is responsible for persisting anything its own implementation needs.

Installed profiles live on the `eUICC` and outlive the module: `cellular_esim_lpa_exit()` neither disables nor removes a profile, and only `cellular_esim_delete_profile()` removes one. A deletion is **permanent** and survives tear-down and any later bring-up.

## Non functional requirements

The following non-functional requirements apply to any implementation of this interface.

### Logging and debugging requirements

The LPA HAL component is required to record all errors and critical informative messages, so that a failure can be identified and the functional flow of the system understood after the fact. Logging should be implemented using the `syslog` method, which provides the capabilities suited to system-level software. The use of `printf` is discouraged unless `syslog` is not available.

All HAL components must adhere to a consistent logging process. When logging is necessary it should be written to the `lpa_vendor_hal.log` file, located in either the `/var/tmp/` or the `/rdklogs/logs/` directory.

Log levels follow the Linux standard logging system, in descending order of severity: `FATAL`, `ERROR`, `WARNING`, `NOTICE`, `INFO`, `DEBUG` and `TRACE`. Each log entry carries a timestamp, the log level and a message describing the event or condition.

Implementations must not log credentials, secrets, security-sensitive information, or other sensitive data.

That rule applies at every log level, `DEBUG` and `TRACE` included, and to every diagnostic output: log files, `syslog`, standard output and standard error, traces, crash dumps, telemetry and error messages. A fragment, hash or length of a sensitive value is treated as the value itself. Where a diagnostic must refer to such a value, it records the operation and its outcome and substitutes the fixed marker `[REDACTED]` for the value. Callers apply the same rule to their own diagnostics.

### Memory and performance requirements

An implementation should not contribute disproportionately to memory or CPU utilization during normal operation, and its resource use should be commensurate with the operation requested. `cellular_esim_get_profile_info()` allocates on behalf of the caller, so its cost grows with the number of profiles installed on the `eUICC`.

### Quality Control

An implementation should pass analysis with third-party tools such as `Coverity`, `Black Duck` and `Valgrind` without reported issues. Neither the HAL implementation nor the vendor software beneath it may introduce memory leaks or memory corruption.

This HAL is not available on the HUB6 or XER10 reference platforms, so the runtime behaviour this document describes has not been observed on reference hardware.

Any change to a file listed under `Source References` obliges a review of this document by the code owner listed in `.github/CODEOWNERS`.

### Licensing

The interface definition is licensed under the Apache License 2.0, with the full text in `LICENSE` and the attribution notice in `NOTICE`. An implementation of this interface is expected to be released under the Apache License 2.0.

### Build Requirements

The source code should be capable of, but not limited to, building under the Yocto distribution environment. The recipe should deliver a shared library named `libesim_lpa.so`.

### Variability Management

Changes to the interface are controlled by versioning. Vendors implement a designated version of the interface and move to later versions as their service agreements require. Each interface version follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html), and vendor code complies with a specific version.

The interface version is identified by the repository's release tag.

### Platform or Product Customization

Product-specific behaviour is confined to the vendor implementation behind the interface.

## Interface API Documentation

All HAL function prototypes and datatype definitions are available in the `lpa_hal.h` header file.

1. Components and processes must include `lpa_hal.h` to make use of LPA HAL capabilities.
2. Components and processes should add a linker dependency for `libesim_lpa.so`.

### Theory of operation and key concepts

The interface has one central concept: the `eSIM` profile, an operator subscription installed onto the `eUICC` and identified by its `ICCID`. Every operation either creates a profile, changes which profile is active, removes a profile, reports on the profiles present, or reports on the card that holds them.

#### Object Lifecycles

- **The profile:** A profile is created on the card by one of the three download functions, enumerated by `cellular_esim_get_profile_info()`, activated and deactivated by `cellular_esim_enable_profile()` and `cellular_esim_disable_profile()`, and permanently removed by `cellular_esim_delete_profile()`. Its `ICCID` is the only handle this interface offers for it.
- **The profile record:** `cellular_esim_get_profile_info()` returns a caller-owned array of `eSIMProfileStruct` describing the profiles found, together with a count. The array is a snapshot, so a caller that has enabled, disabled or deleted a profile, or brought the module up again, re-reads rather than reuses it.
- **The module:** `cellular_esim_lpa_init()` brings the Local Profile Assistant up and `cellular_esim_lpa_exit()` tears it down. Neither returns a handle or a context, so the module state is implicit and at most one instance of it is addressable through this interface.

#### Method Sequencing

- `cellular_esim_lpa_init()` **comes first**, before any other function in this interface, including `cellular_esim_get_eid()` and `cellular_esim_get_euicc()`.
- **Download before lifecycle:** A profile must have been downloaded, or already be present on the card, before `cellular_esim_enable_profile()`, `cellular_esim_disable_profile()` or `cellular_esim_delete_profile()` can succeed.
- **Inspect to obtain an `ICCID`:** `cellular_esim_get_profile_info()` is how a caller discovers the `ICCID` values it may pass to those three calls.
- `cellular_esim_lpa_exit()` **comes last**, before shutdown or once the functionality is no longer required.

**Download arguments:** The syntax of an activation code is fixed by the issuing party or the vendor implementation. The header illustrates the other two arguments with test endpoints rather than live operator infrastructure: an `SM-DS` address such as `oem-smds-json.demo.gemalto.com` and an `SM-DP+` address such as `smdp-plus.test.gsma.com`. The values a deployment uses arrive from provisioning at run time.

#### State-Dependent Behavior

- **Profile state:** The only state this interface exposes is `eSIMProfileStruct.profileState`; `State Diagram` gives its values and the effect of each lifecycle call on it.
- **Outcome depends on the card's contents:** All three lifecycle calls verify the supplied `ICCID` against the profiles stored on the card and fail when there is no match, so their outcome depends on what is installed rather than on a mode the interface exposes.
- **After download:** A caller that needs a newly downloaded profile enabled calls `cellular_esim_enable_profile()` explicitly rather than assuming the download left it active.

### Data Structures and Defines

The header organizes its public types and functions under the Doxygen groups `lpa_hal`, `LPA_HAL_TYPES` and `LPA_HAL_APIS`, so the generated reference presents the types and the functions on separate pages.

| Definition | Kind | Represents |
| --- | --- | --- |
| `eSIMProfileStruct` | struct | One `eSIM` profile as reported by the card. |
| `cellular_sim_download_progress_callback` | function pointer typedef | Progress notification during an activation-code download, installed by `cellular_esim_download_profile_with_activationcode()`; see `Asynchronous Notification Model`. |
| `CellularPreferredUICC_t` | enumeration | Which `UICC` type is preferred: a removable `USIM` or the `eUICC`. |
| `CellularDeviceSimStatus_t` | enumeration | Preparation status of a device `SIM`. |

`eSIMProfileStruct` is the only structure this interface defines, and it is populated by the implementation and read by the caller — never the other way round.

| Member | Type | Constraint | Meaning |
| --- | --- | --- | --- |
| `iccid` | `char[32]` | Value is a 20-digit string; the buffer is 32 bytes | Profile identifier, in the format of `ITU-T` Recommendation E.118. |
| `profileState` | `int` | `0` or `1` | Whether the profile is currently active: `0` disabled, `1` enabled. |
| `profileName` | `char[16]` | Zero-terminated | Human-readable operator name, for example `Xfinity Mobile`. |

The buffer sizes are the contract a caller must respect: a caller takes the `ICCID` length from the 20-digit contract rather than from the field size, and an operator name longer than the 16-byte field, terminator included, cannot be represented.

**Status macros:** `RETURN_ERROR` (`-1`), `RETURN_OK` (`0`), `TRUE` (`1`) and `FALSE` (`0`) are the values the functions return; `Internal Error Handling` gives the meaning of each. `TRUE` and `FALSE` are each defined only where the including translation unit has not already defined them.

**The two enumerations are not used by any function:** `CellularPreferredUICC_t` (`CELLULAR_UICC_USIM` = 0, `CELLULAR_UICC_ESIM`) and `CellularDeviceSimStatus_t` (`SIM_PREPARATION_PENDING` = 1, `SIM_PREPARATION_INPROGRESS`, `SIM_PREPARATION_DOWNLOADED`, `SIM_PREPARATION_DONE`) appear in no declared function signature, so a caller can construct their values but has nowhere in this interface to pass them.

### API Surface

This interface declares **eleven** functions, all listed below by exact identifier. Per-API detail — argument ranges, pre-conditions, every return value and what a caller should do about each — is carried by the Doxygen block on each declaration in `include/lpa_hal.h`.

**Module lifecycle:**

| API | Purpose |
| --- | --- |
| `cellular_esim_lpa_init` | Initializes the Local Profile Assistant, establishes modem readiness and sets up the parameters the other operations need. Mandatory before any other call. |
| `cellular_esim_lpa_exit` | Deactivates the module and releases its resources. |

**Profile download:**

| API | Purpose |
| --- | --- |
| `cellular_esim_download_profile_with_activationcode` | Downloads and installs a profile from an activation code, reporting progress through a caller-supplied callback. |
| `cellular_esim_download_profile_from_smds` | Downloads a profile by connecting to a specified `SM-DS` address. Reports no progress. |
| `cellular_esim_download_profile_from_defaultsmdp` | Downloads a profile from a default `SM-DP+` address. Reports no progress. |

**Profile lifecycle:**

| API | Purpose |
| --- | --- |
| `cellular_esim_enable_profile` | Activates the profile whose `ICCID` matches the one supplied. |
| `cellular_esim_disable_profile` | Deactivates the profile whose `ICCID` matches the one supplied. |
| `cellular_esim_delete_profile` | Permanently removes the profile whose `ICCID` matches the one supplied. |

**Profile inspection:**

| API | Purpose |
| --- | --- |
| `cellular_esim_get_profile_info` | Retrieves the `MNO` profiles present on the card as a callee-allocated, caller-freed array, with a count. |

**Card identity:**

| API | Purpose |
| --- | --- |
| `cellular_esim_get_eid` | Retrieves the `EID` of the device's `eSIM`. |
| `cellular_esim_get_euicc` | Retrieves information about the `eUICC`. |

`cellular_esim_get_eid()` and `cellular_esim_get_euicc()` are each declared with an **empty parameter list** and return only a status code, so they hand no `EID` or `eUICC` information back to the caller through this interface.

### Sequence Diagram

The exchange below walks the whole interface in a valid order, from bring-up to tear-down, using the real identifiers.

```mermaid
sequenceDiagram
    participant Caller as RDK-B Middleware
    participant HAL as LPA HAL
    participant Vendor as Vendor Software
    Caller->>HAL: cellular_esim_lpa_init()
    HAL->>Vendor: Configure LPA module, confirm modem readiness
    Vendor-->>HAL: eUICC and modem present
    HAL-->>Caller: RETURN_OK
    Caller->>HAL: cellular_esim_get_eid()
    HAL->>Vendor: Read EID from the eUICC
    Vendor-->>HAL: EID read
    HAL-->>Caller: RETURN_OK
    Caller->>HAL: cellular_esim_get_euicc()
    HAL->>Vendor: Read eUICC information
    Vendor-->>HAL: eUICC information read
    HAL-->>Caller: RETURN_OK
    Caller->>HAL: cellular_esim_download_profile_with_activationcode(code, callback)
    HAL->>Vendor: Decode activation code, authenticate with SM-DP+
    HAL-->>Caller: callback: Activation Code decoded
    Vendor-->>HAL: Bound Profile Package installed
    HAL-->>Caller: callback: Profile download successful
    HAL-->>Caller: TRUE
    Caller->>HAL: cellular_esim_download_profile_from_smds(smds)
    HAL->>Vendor: Connect to SM-DS address, download profile
    Vendor-->>HAL: Profile downloaded, no progress reported
    HAL-->>Caller: RETURN_OK
    Caller->>HAL: cellular_esim_download_profile_from_defaultsmdp(smdp)
    HAL->>Vendor: Connect to default SM-DP+ address, download profile
    Vendor-->>HAL: Profile downloaded, no progress reported
    HAL-->>Caller: RETURN_OK
    Caller->>HAL: cellular_esim_get_profile_info(&list, &count)
    HAL->>Vendor: Enumerate installed MNO profiles
    Vendor-->>HAL: Profile records with ICCID, state and name
    HAL-->>Caller: RETURN_OK, caller must free the array
    Caller->>HAL: cellular_esim_enable_profile(iccid, 20)
    HAL->>Vendor: Match ICCID, activate profile
    Vendor-->>HAL: Profile enabled
    HAL-->>Caller: RETURN_OK
    Caller->>HAL: cellular_esim_disable_profile(iccid, 20)
    HAL->>Vendor: Match ICCID, deactivate profile
    Vendor-->>HAL: Profile disabled
    HAL-->>Caller: RETURN_OK
    Caller->>HAL: cellular_esim_delete_profile(iccid, 20)
    HAL->>Vendor: Match ICCID, remove profile from the eUICC
    Vendor-->>HAL: Profile deleted
    HAL-->>Caller: RETURN_OK
    Caller->>HAL: cellular_esim_lpa_exit()
    HAL->>Vendor: Release LPA resources
    Vendor-->>HAL: Resources released
    HAL-->>Caller: RETURN_OK
```

The three download calls are alternative paths, and the three profile lifecycle calls are shown consecutively only to name each one; neither order is a requirement, and `Method Sequencing` gives the required ordering. The `TRUE` return on the activation-code path is that declaration's documented convention; see `Internal Error Handling`.

### State Diagram

`eSIMProfileStruct.profileState` is the only state any declaration reports. The diagram shows the profile lifecycle: a download installs a profile on the `eUICC`, `cellular_esim_enable_profile()` and `cellular_esim_disable_profile()` move it between the two `profileState` values, and `cellular_esim_delete_profile()` removes it. Each edge is a step of that lifecycle, and the table below gives the documented effect of each successful call. A download ends at the installed state rather than at either value; `State-Dependent Behavior` gives what a caller does next.

```mermaid
stateDiagram-v2
    direction LR
    state "Installed on the eUICC" as Installed {
        state "profileState = 0 (disabled)" as Disabled
        state "profileState = 1 (enabled)" as Enabled
        Disabled --> Enabled : cellular_esim_enable_profile()
        Enabled --> Disabled : cellular_esim_disable_profile()
    }
    [*] --> Installed : download (any of three paths)
    Installed --> [*] : cellular_esim_delete_profile()
```

| Operation | Documented effect of success |
| --- | --- |
| `cellular_esim_enable_profile` | The named profile is active, and a subsequent `cellular_esim_get_profile_info()` reports its `profileState` as `1`. |
| `cellular_esim_disable_profile` | The named profile is no longer active and reports `profileState` as `0`. It remains installed — disabling is not deletion. |
| `cellular_esim_delete_profile` | The profile is removed from the card permanently and no longer appears in `cellular_esim_get_profile_info()`. |
| the three download functions | A profile has been downloaded or its download initiated; the profile inventory confirms what is on the card. |

A caller treats any `profileState` value other than `0` or `1` as an unknown state, re-reading the inventory rather than mapping the value onto enabled or disabled or acting on it. `cellular_esim_lpa_init()` and `cellular_esim_lpa_exit()` change whether the module is usable, not the value of `profileState`; tear-down leaves installed profiles unchanged (see `Persistence Model`).

`CellularDeviceSimStatus_t` (see `Data Structures and Defines`) and the download progress messages (see `Asynchronous Notification Model`) are not profile states and are not drawn.

## Source References

- [rdkcentral/rdkb-halif-lpa/include/lpa_hal.h](https://github.com/rdkcentral/rdkb-halif-lpa/blob/develop/include/lpa_hal.h) — interface header: declarations, types, status codes and per-API documentation
- [rdkcentral/rdkb-halif-lpa/CHANGELOG.md](https://github.com/rdkcentral/rdkb-halif-lpa/blob/develop/CHANGELOG.md) — release history behind `Version History`
- [rdkcentral/rdkb-halif-lpa/LICENSE](https://github.com/rdkcentral/rdkb-halif-lpa/blob/develop/LICENSE) — Apache License 2.0 text
- [rdkcentral/rdkb-halif-lpa/NOTICE](https://github.com/rdkcentral/rdkb-halif-lpa/blob/develop/NOTICE) — attribution notice
- [rdkcentral/rdkb-halif-lpa/COPYING](https://github.com/rdkcentral/rdkb-halif-lpa/blob/develop/COPYING) — Apache License 2.0 text, a symbolic link to `LICENSE`
- [rdkcentral/rdkb-halif-lpa/.github/CODEOWNERS](https://github.com/rdkcentral/rdkb-halif-lpa/blob/develop/.github/CODEOWNERS) — responsible reviewer named under `Quality Control`

