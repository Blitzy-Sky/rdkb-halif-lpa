/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
* @file lpa_hal.h
* @brief Interface definition for the RDK-B LPA (Local Profile Assistant) HAL: eSIM
*        profile management on an eUICC.
*
* This header is the complete LPA HAL contract. It declares the status macros an
* implementation reports through, the profile record a caller reads, the download
* progress callback type, and the eleven functions that download, enumerate, enable,
* disable and delete eSIM profiles on the eUICC and that bring the Local Profile
* Assistant up and tear it down. The declared surface is eSIM and eUICC management
* and nothing else: no declaration in this header concerns power management, and
* `Description` in the repository specification records the
* same surface.
*
* The interface is caller-driven and synchronous. Every function returns int and
* reports its outcome through that return value, and the one asynchronous channel -
* the download progress callback - is passed per call rather than registered, which
* the repository specification states under `Asynchronous Notification Model`.
*
* Three properties of the return vocabulary govern every declaration below, and a
* caller cannot write a correct status test without them.
*
* 1. The interface defines exactly two status codes, RETURN_OK (0) and RETURN_ERROR
*    (-1), and no error-code retrieval call. The status code is the only failure
*    information this interface defines, as the repository specification states under
*    `Internal Error Handling`; the reason behind a failure is available only from the
*    vendor log named under `Logging and debugging requirements`.
* 2. cellular_esim_download_profile_with_activationcode() is documented against a
*    different vocabulary from the other ten functions - TRUE (1) for success and
*    FALSE (0) for failure - and because FALSE and RETURN_OK are the same integer, a
*    status test written for the other ten reads its documented failure as a success.
*    There is therefore no single comparison that is correct across this whole
*    interface. The collision is stated again on that declaration, on the macros
*    themselves, and under `Internal Error Handling`.
* 3. Neither vocabulary is refined further. There is no second failure code, no error
*    class and no per-error detail, so a caller learns that an operation failed and
*    nothing else about why.
*
* Behaviour stated in this header is derived from these declarations and from the
* repository specification. Nothing is presented as observed runtime behaviour, and
* where neither source establishes a behaviour this header says so rather than
* supplying a plausible value.
*/

#ifndef _LPA_HAL_H_
#define _LPA_HAL_H_

/**********************************************************************
                ENUMERATION DEFINITIONS
**********************************************************************/
/**
 * @ingroup LPA_HAL_TYPES
 * @brief Which class of UICC (Universal Integrated Circuit Card) is preferred for a
 *        subscription: a removable card, or the embedded card this interface manages.
 *
 * @note No function declared in this header takes or returns this type, and none of
 *       the declared structures contains it. The repository specification records the
 *       same under `Data Structures and Defines` in the HAL specification: a caller
 *       can construct a value but has nowhere in this interface to pass it, so no
 *       behaviour in this interface depends on it and none is asserted for it here.
 * @note The names in this interface are not drawn from one convention - an enumeration
 *       tag of the form _Cellular_PreferredUICC_t against a typedef named
 *       CellularPreferredUICC_t, a structure named eSIMProfileStruct with no _t suffix,
 *       and functions named cellular_esim_* beside a callback type named
 *       cellular_sim_*. Every one of those names is part of the published contract and
 *       is used exactly as declared; renaming any of them would break every caller
 *       compiled against this header, so none is renamed and a caller should not expect
 *       a uniform scheme.
 */
typedef enum _Cellular_PreferredUICC_t
{
    CELLULAR_UICC_USIM     = 0,  /*!< Removable Universal Integrated Circuit Card carrying a Universal Subscriber Identity Module, that is, a physical SIM inserted into the device. The enumeration is explicitly 0-based at this member, so a zero-initialised field of this type reads as this value. */
    CELLULAR_UICC_ESIM,          /*!< Embedded Universal Integrated Circuit Card - the eUICC onto which the profiles this interface downloads, enables, disables and deletes are installed. It follows CELLULAR_UICC_USIM and so takes the value 1. */
} CellularPreferredUICC_t;

/**
 * @ingroup LPA_HAL_TYPES
 * @brief How far the preparation of a SIM in a cellular device has progressed.
 *
 * @note No function declared in this header takes or returns this type, and none of the
 *       declared structures contains it, which the repository specification also
 *       records under `Data Structures and Defines` in the HAL specification. Nothing
 *       in this interface reports a value of this type to a caller, so a caller has no
 *       way to observe the progress it describes through this interface.
 * @note This interface exposes these values only. It does not state that a SIM moves
 *       from one of them to the next, in what order, or which operations cause a move,
 *       so a caller must not read the members as a state machine and a test must not
 *       assert a transition between them. `State Diagram` in the repository
 *       specification makes the same point and declines to draw them for that reason.
 */
typedef enum _CellularDeviceSimStatus_t
{
   SIM_PREPARATION_PENDING = 1,    /*!< Preparation has been requested and has not started. The enumeration is explicitly 1-based at this member, so no member of this type has the value 0 and a zero-initialised field carries no member of it. */
   SIM_PREPARATION_INPROGRESS,     /*!< Preparation is under way and has not finished; value 2. This interface does not state which operations, if any, are rejected while a SIM is in this condition. */
   SIM_PREPARATION_DOWNLOADED,     /*!< The material needed to prepare the SIM has been downloaded to it, but preparation is not yet complete; value 3. It is not equivalent to SIM_PREPARATION_DONE and does not on its own make a profile usable. */
   SIM_PREPARATION_DONE,           /*!< Preparation has completed; value 4. This is the terminal value of the four, and this interface states no member representing a preparation failure. */
} CellularDeviceSimStatus_t;

/**********************************************************************
                STRUCTURE AND CONSTANT DEFINITIONS
**********************************************************************/
/**
* @defgroup lpa_hal LPA HAL
*
* The LPA (Local Profile Assistant) HAL is the RDK-B interface for eSIM profile
* management on an eUICC. It abstracts a vendor's Local Profile Assistant so that
* RDK-B software can download, enable, disable, delete and enumerate operator profiles,
* and read the identity of the eUICC, without depending on a particular modem, eUICC or
* vendor implementation.
*
* @defgroup LPA_HAL_TYPES  LPA HAL Data Types
* @ingroup  lpa_hal
*
* The status macros a caller compares a return value against, the profile record a
* caller reads, the progress callback type it implements, and the two enumerations this
* interface declares without using in any signature.
*
* @defgroup LPA_HAL_APIS   LPA HAL  APIs
* @ingroup  lpa_hal
*
* The eleven declared functions: module lifecycle, the three alternative download
* paths, profile enable, disable and delete, profile enumeration, and the two eUICC
* identity reads.
*
**/

/**
* @addtogroup LPA_HAL_TYPES
* @{
*/

/**
 * @brief Return values used by the declarations in this interface.
 *
 * These four macros are the whole return vocabulary this interface defines. RETURN_OK
 * and RETURN_ERROR are the two status codes the repository specification lists under
 * `Internal Error Handling` in the HAL specification, and ten of the eleven functions
 * and the progress callback are documented against them. TRUE and FALSE are the values
 * the eleventh, cellular_esim_download_profile_with_activationcode(), is documented
 * against instead.
 *
 * @warning The two vocabularies collide on the value 0: FALSE and RETURN_OK are the
 *          same integer, so a documented failure in the TRUE/FALSE vocabulary is
 *          indistinguishable from a documented success in the RETURN_OK/RETURN_ERROR
 *          one. No single comparison is therefore correct for every function in this
 *          interface, and a caller must apply the vocabulary each declaration
 *          documents rather than one test for all of them. The affected declaration
 *          states the hazard again on itself, and `Internal Error Handling` sets out
 *          the caller action.
 * @note RETURN_ERR is not defined here, and not by anything this header includes. The
 *       defined failure code is RETURN_ERROR, and a comparison written against
 *       RETURN_ERR does not compile.
 * @note TRUE and FALSE are each guarded by an ifndef, so where a translation unit has
 *       already defined either of them this header leaves that definition in place. A caller
 *       that has defined them to values other than 1 and 0 changes what the affected
 *       declaration's documented returns mean, and nothing in this interface detects
 *       that. RETURN_ERROR and RETURN_OK are unguarded, so a translation unit that has
 *       already defined either of those names does not compile against this header.
 * @note This interface defines no version macro, so a caller cannot test at compile
 *       time or at run time which revision of this contract it is building against;
 *       the repository specification records that under `Variability Management`.
 */
#define RETURN_ERROR        (-1)   /*!< Failure, in the vocabulary the ten functions other than cellular_esim_download_profile_with_activationcode() are documented against. The requested operation did not complete, and no output parameter may be relied on. The interface gives no reason for the failure beyond this value. */
#define RETURN_OK           (0)    /*!< Success, in the same vocabulary. The requested operation completed and the post-conditions on the declaration hold. This value equals FALSE, which is the collision the warning above describes. */
#ifndef TRUE
#define TRUE                (1)    /*!< Success, in the vocabulary cellular_esim_download_profile_with_activationcode() alone is documented against. It is a non-zero value, so it is not equal to RETURN_OK. */
#endif
#ifndef FALSE
#define FALSE               (0)    /*!< Failure, in that same one-declaration vocabulary. It equals RETURN_OK, so a caller that tests the affected function against RETURN_OK reads a documented failure as a success. */
#endif

/**
 * @brief One eSIM profile as reported by the eUICC: its identifier, whether it is
 *        active, and the operator name it carries.
 *
 * This is the record cellular_esim_get_profile_info() populates, one element per
 * profile installed on the card. It is written by the implementation and read by the
 * caller and never the other way round: no function in this interface accepts a
 * populated eSIMProfileStruct, so a caller cannot use it to request a change. To act on
 * a profile a caller passes the iccid value it read here to
 * cellular_esim_enable_profile(), cellular_esim_disable_profile() or
 * cellular_esim_delete_profile().
 *
 * The three fields are fixed-size, and those sizes are the contract: a caller bounds
 * every read by the declared size and must not infer the length of a value from the
 * size of the field carrying it.
 *
 * @note The array of these records is allocated by cellular_esim_get_profile_info() and
 *       released by the caller. That is the only allocation this interface hands to a
 *       caller, and the repository specification states it under `Memory Model`
 *       in the HAL specification.
 * @note The record is a snapshot of the card at the moment it was read. This interface
 *       does not state that it is updated when the card changes, so a caller that has
 *       enabled, disabled or deleted a profile re-reads rather than reusing it.
 * @see cellular_esim_get_profile_info
 * @see cellular_esim_enable_profile
 */
typedef  struct
{
    char iccid[32];          /*!< ICCID (Integrated Circuit Card Identification) of this profile, in the format of ITU-T Recommendation E.118, and the only handle this interface offers for addressing a profile. The value is a 20-digit string while the field is 32 bytes, so the field is wider than the value it carries: a caller bounds reads by 32 bytes and takes the length of the value from the 20-digit contract stated on cellular_esim_enable_profile(), not from the field size. This interface does not state whether the implementation zero-terminates the value or what the remaining bytes contain. */
    int  profileState;       /*!< Whether this profile is currently active: 00 (disabled) or 01 (enabled). Those two values are the whole documented domain of the field, and cellular_esim_enable_profile() and cellular_esim_disable_profile() are what move a profile between them. This interface does not state the value carried by a profile that has just been downloaded, so a caller that needs the profile active enables it explicitly rather than inferring it from a download. */
    char profileName[16];    /*!< Human-readable operator name of the profile, for example "Xfinity Mobile", "Comcast" or "CRTC". The field is 16 bytes including any terminator, so an operator name longer than that cannot be represented in it; a caller bounds reads by 16 bytes and must not assume the name is complete. This interface does not define a vocabulary of names, so the value is compared as vendor-supplied text rather than matched against a fixed set. */
} eSIMProfileStruct;

/**
* @}
*/

/**
* @addtogroup LPA_HAL_APIS
* @{
*/

/**********************************************************************
                FUNCTION PROTOTYPES
**********************************************************************/


/*
 * Return-code contract for the declarations below.
 *
 * Every function reports its outcome synchronously through its int return value. No
 * function reports a failure by any other route, and none of them has an out-of-band
 * error channel: the repository specification states under `Internal Error Handling`
 * in the HAL specification that the status code is the only failure information this
 * interface defines. There is no second failure code, no error class, no errno-style
 * variable and no retrieval call, so a caller learns that an operation failed and
 * nothing further about why; the reason is recoverable only from the vendor log
 * described under `Logging and debugging requirements`. Each declaration below
 * therefore documents the client action for its failure value rather than a diagnosis.
 *
 * Two vocabularies are in use, and they are not interchangeable. Ten of the eleven
 * functions, and cellular_sim_download_progress_callback, are documented against
 * RETURN_OK (0) for success and RETURN_ERROR (-1) for failure.
 * cellular_esim_download_profile_with_activationcode() is documented against TRUE (1)
 * and FALSE (0) instead, and because FALSE and RETURN_OK are the same integer a caller
 * must not share one comparison between it and the other ten. The declaration and the
 * macro definitions above both state the hazard.
 */

/**
 * @ingroup LPA_HAL_TYPES
 * @brief Reports the progress of an eSIM profile download to the caller that started it.
 *
 * A caller implements a function of this type and passes it to
 * cellular_esim_download_profile_with_activationcode(); the implementation invokes it
 * once per stage it reaches while installing the profile, from decoding the activation
 * code through authentication to the final download. It is the only asynchronous
 * channel in this interface, and it is installed per call rather than registered:
 * there is no registration or deregistration function here, so a caller has nothing
 * to unregister and no way to withdraw the function once it has been passed. The
 * repository specification states this under `Asynchronous Notification Model`
 * in the HAL specification, and notes that the other two download paths accept no
 * callback and therefore report no intermediate progress at all.
 *
 * @param[in] progress Status message naming the stage reached. The pointer addresses
 *                     storage the implementation owns, and this interface does not state
 *                     that it stays valid once the callback returns, so a body copies
 *                     what it needs and must not retain, free or write through the
 *                     pointer, even though it is declared char* rather than const char*.
 *                     No length accompanies the pointer and this interface does not
 *                     state that the message is terminated, so no readable extent is
 *                     established here at all: a body must not read or copy it with an
 *                     unbounded string operation, and caps every copy at the size of its
 *                     own buffer. The declared message values are "Activation Code
 *                     decoded", "Initiate authentication done", "SM-DP+ address checking
 *                     done", "Authenticate server done", "Authenticate client done",
 *                     "PPR conditions check done", "Get Bound Profile Package done",
 *                     "Send PIR notification to server done", "Clear PIR notification
 *                     done", "Read pending notifications...", "<1> pending
 *                     notification(s) detected", "Processing notification #1 <33>",
 *                     "Notification <33> successfully sent", "Load Bound Profile Package
 *                     done - Profile download successful" and "download successful". Two
 *                     embed values that vary at run time - the pending-notification
 *                     count and the notification identifier - so a caller matches those
 *                     on a prefix. This interface states neither the order of delivery
 *                     nor that every message appears in every download, so the list is
 *                     not a sequence and a missing message is not a failure. The
 *                     behaviour for a NULL pointer is not specified by this interface.
 *
 * @pre Everything the callback body touches must be valid before the caller makes the
 *      cellular_esim_download_profile_with_activationcode() call that carries this
 *      function, and must stay valid afterwards. This interface does not establish that
 *      the return of that call ends delivery: it defines no deregistration call, names
 *      no final message, and its own post-condition allows the call to return before
 *      installation is complete, so a caller must not treat the call's return as the
 *      end of the arrangement. The lifetime warning below states how long the callback
 *      and its state must therefore be kept.
 * @post Returning RETURN_OK reports that the caller accepted the message; the
 *       implementation continues with the download. Returning RETURN_ERROR reports that
 *       it did not, and this interface does not state what an implementation does with
 *       that - so a caller must not assume that returning a failure cancels, pauses or
 *       otherwise alters the download, and must not use it as a way to abort one.
 *
 * @returns int - the status the caller reports back to the implementation for the
 *          message just delivered. The download's own outcome is the return value of
 *          cellular_esim_download_profile_with_activationcode() and is not reported
 *          here.
 * @retval RETURN_OK - The message was accepted.
 * @retval RETURN_ERROR - The caller could not accept the message, for example because
 *         it was not in a form the caller recognised. The consequence is not specified
 *         by this interface; see the post-condition above.
 *
 * @warning Lifetime: this interface fixes no point at which delivery stops, so a caller
 *          keeps this function callable, and the state its body touches alive, until it
 *          has established by other means that no further invocation can occur -
 *          conservatively until cellular_esim_lpa_exit() has returned. Releasing either
 *          when the download call returns risks an invocation through a dangling pointer.
 * @note Blocking: a body that blocks holds up the download that invoked it, and
 *       `Blocking calls` warns against long-running work on the calling thread for that
 *       reason. This interface bounds nothing here - no maximum callback duration, and
 *       nothing establishing that an implementation abandons or reschedules a slow
 *       callback - so a caller returns promptly and defers lengthy work to its own context.
 * @note Thread safety: this interface does not specify which thread the callback is
 *       invoked on, nor whether it may be invoked concurrently with the download call
 *       that installed it, and `Threading Model` records that gap. A caller writes the
 *       body as though it runs on a thread it does not own, serialises its own access to
 *       whatever it touches, and must not assume that re-entering this interface is safe.
 * @see cellular_esim_download_profile_with_activationcode
 */
typedef int (*cellular_sim_download_progress_callback)( char *progress );

/**
* @brief Downloads and installs an eSIM profile identified by an activation code.
*
* Decodes the supplied activation code, reaches the SM-DP+ server that holds the
* prepared profile for it, authenticates with that server and installs the profile
* onto the eUICC, reporting each stage it reaches through the caller-supplied progress
* callback. It is one of three alternative download paths in this interface - the
* other two are cellular_esim_download_profile_from_smds() and
* cellular_esim_download_profile_from_defaultsmdp() - and it is the only one that
* reports progress, which the repository specification states under `Optional
* Components` in the HAL specification.
*
* @param[in] ActivationCodeStr Activation code identifying the profile to download and
*                              the server that holds it. Its syntax is not established
*                              by this interface: no field list, no separator, no
*                              character set and no maximum length is stated here, and
*                              this repository cites no provisioning specification
*                              that states one, so the grammar a code must satisfy is
*                              fixed by the specification the eSIM ecosystem it comes
*                              from applies, by the party that issued the code, or by
*                              the vendor implementation behind this call - not here.
*                              A caller therefore passes an issued code through
*                              unchanged rather than constructing, normalising or
*                              validating one against a rule this interface defines,
*                              and cannot conclude from anything stated here that a
*                              code its implementation rejects was malformed. What
*                              this header does record is narrower: the progress
*                              strings documented on
*                              cellular_sim_download_progress_callback show that a
*                              code is decoded and that an SM-DP+ address is checked
*                              in the course of the download.
*                              The caller allocates the buffer and owns it throughout,
*                              and it is the caller that eventually releases it -
*                              though not on the strength of this call returning, for
*                              the reason below; the `Memory Model` topic of the
*                              repository specification places both the allocation and
*                              the zero termination on the caller, and termination is
*                              what allows the length to be determined at all, since
*                              this interface passes no length alongside the pointer.
*                              This interface does not specify whether the
*                              implementation retains the pointer beyond the call, and
*                              it declares no call through which a retained pointer
*                              could be withdrawn, so the caller keeps ownership but
*                              must not read the return as permission to reuse or
*                              release the buffer; equally, the parameter is declared
*                              char* rather than const char*, so this interface does
*                              not establish that the implementation leaves the buffer
*                              unmodified. The behaviour for a NULL pointer or an
*                              empty string is not specified by this interface.
* @param[in] download_progress Function of type
*                              cellular_sim_download_progress_callback that receives
*                              one status message per stage of the download. It is
*                              supplied per call, and there is no registration or
*                              deregistration function in this interface, so a caller
*                              cannot withdraw it once it has been passed. The caller
*                              owns the code the pointer addresses and must keep it
*                              callable, and the state its body touches alive, past the
*                              return of this call: the post-condition below allows this
*                              function to return before installation is complete, so
*                              the return does not establish that the last invocation
*                              has happened. This interface does not state whether NULL
*                              may be passed to decline progress reporting. The message
*                              values and the callback's own return are on the typedef.
*
* @pre cellular_esim_lpa_init() must have completed successfully. `Initialization and
*      Startup` makes it mandatory before any other function in this interface. This
*      interface does not state what this function does when it has not been called, so
*      a caller must not rely on a failure value to detect the omission.
* @post On the documented success value the download has been initiated with a modem
*       present. Note what that does not say: this interface does not state that
*       installation is complete when the call returns, so a caller that needs
*       certainty either waits for the final progress message or re-reads the card with
*       cellular_esim_get_profile_info(). This interface also does not state the value
*       of eSIMProfileStruct.profileState for a newly installed profile, so a caller
*       that needs the profile active calls cellular_esim_enable_profile() explicitly.
* @post On the documented failure value no output is produced and this interface does
*       not state whether a partially installed profile can remain on the eUICC; a
*       caller that must know enumerates the card with
*       cellular_esim_get_profile_info() rather than assuming the card is unchanged.
*
* @returns int - the outcome of the download, in the TRUE/FALSE vocabulary this
*          declaration alone uses.
* @retval TRUE - (1) The download was initiated successfully and a modem is present.
*         The client proceeds to follow the progress messages, or re-reads the card, as
*         the post-condition above describes.
* @retval FALSE - (0) The download could not be initiated, or no modem is present. The
*         client treats no profile as having been installed until it has confirmed
*         otherwise by enumeration, and consults the vendor log for the reason; this
*         interface supplies none. Retrying is a caller decision: no retry policy,
*         count or interval is specified by this interface.
*
* @warning The success and failure values of this one declaration collide with the
*          vocabulary the other ten functions use. FALSE and RETURN_OK are both 0, so a
*          caller that tests this result against RETURN_OK reads a documented failure as
*          a success, and one that tests it against RETURN_ERROR (-1) never sees a
*          failure at all. This interface does not establish which vocabulary an
*          implementation actually follows here - this declaration documents TRUE and
*          FALSE, every other declaration documents RETURN_OK and RETURN_ERROR, and
*          nothing in this repository resolves the two. A caller must therefore treat a
*          non-zero result as success only against a convention it has confirmed with
*          the vendor of the implementation it is calling, and must not reuse the
*          comparison that is correct for the other ten functions. `Internal Error
*          Handling` in the repository specification states the same position.
* @warning An activation code is a one-time credential, not an identifier: it authorises
*          the download of one prepared profile, so a party that obtains the string can
*          attempt that download. Handle it as a
*          secret. Keep it out of logs, traces, crash dumps and error messages -
*          including any diagnostic that prints call arguments generically - and do not
*          pass it to a logging helper as an aid to debugging; the value belongs in
*          neither lpa_vendor_hal.log nor a caller's own log, and `Logging and debugging
*          requirements` states the same position. Overwrite the buffer before it is
*          reused or released rather than letting the characters persist in freed
*          storage, having first established the retention behaviour described on
*          ActivationCodeStr above, since this interface does not settle when the
*          implementation stops reading the buffer. This interface performs no redaction
*          of its own and states nothing about whether an implementation logs, stores or
*          transmits the value beyond the download it authorises.
* @note Failure detail: this interface offers none. It declares no error-description
*       call: lpaGetErrorCodeDescription() and lpaGetErrorCode() are declared neither
*       by this header nor anywhere else in this repository, so code written against
*       them does not link. The return value is the only failure information available
*       at the call site, and `Logging and debugging requirements` names the vendor
*       log, lpa_vendor_hal.log under /var/tmp/ or /rdklogs/logs/, as where the reason
*       is recorded.
* @note Blocking: the call is synchronous - the outcome is the return value, and no part
*       of the result arrives later except through the progress callback. It contacts a
*       remote SM-DP+ server, so its duration depends on a network round trip rather
*       than on local computation. Even so, this interface does not specify whether it
*       may block, and `Blocking calls` states that explicitly for all three download
*       paths; a caller must not assume either behaviour. No timeout, completion-time
*       target or upper bound is specified anywhere in this repository, so a caller that
*       cannot tolerate an unbounded wait imposes and enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading
*       Model` places on the calling module the obligation to serialise all of its
*       interactions with these APIs, so a caller must not invoke this function
*       concurrently with any other function in this interface. The functions are
*       expected to be callable from several processes at once, so a vendor
*       implementation must also protect its own state against concurrent access from
*       different processes. The thread on which download_progress is invoked is not
*       specified by this interface; see the typedef.
* @see cellular_sim_download_progress_callback
* @see cellular_esim_download_profile_from_smds
* @see cellular_esim_download_profile_from_defaultsmdp
* @see cellular_esim_get_profile_info
* @see cellular_esim_enable_profile
* @see cellular_esim_lpa_init
*/
int cellular_esim_download_profile_with_activationcode(char* ActivationCodeStr, cellular_sim_download_progress_callback download_progress);

/**
* @brief Downloads an eSIM profile by discovering it at an SM-DS address.
*
* Connects to the Subscription Manager Discovery Service the caller names, obtains the
* profile it advertises for this eUICC and downloads it. It is one of the three
* alternative download paths in this interface and reports no intermediate progress: it
* takes no callback argument, so the return value is the only outcome a caller sees.
* All three download paths are declared unconditionally - no feature macro and no
* conditional block removes any of them - so a caller chooses between alternatives
* rather than testing for a path's presence; `Optional Components` states the same.
*
* @param[in] smds SM-DS address in URL form, for example
*    @code "oem-smds-json.demo.gemalto.com" @endcode
*                 The caller allocates the buffer and owns it throughout, and it is
*                 the caller that eventually releases it - though not on the strength
*                 of this call returning, for the reason below; `Memory Model` places
*                 both the
*                 allocation and the zero termination on the caller, and the termination
*                 is the only way the length can be determined, since no length
*                 accompanies the pointer. This interface states no maximum length, no
*                 scheme prefix and no validation rule beyond the worked value above, so
*                 a caller cannot check the address against a rule this interface
*                 defines. This interface does not specify whether the implementation
*                 retains the pointer beyond the call, and it declares no call through
*                 which a retained pointer could be withdrawn, so the caller keeps
*                 ownership but must not read the return as permission to reuse or
*                 release the buffer; equally, the parameter is declared char* rather
*                 than const char*, so this interface does not establish that the buffer
*                 is left unmodified. The behaviour for a NULL pointer or an empty string
*                 is not specified by this interface.
*
* @pre cellular_esim_lpa_init() must have completed successfully; `Initialization and
*      Startup` makes it mandatory before any other function in this interface. This
*      interface does not state what this function does when it has not been called, so a
*      caller must not rely on RETURN_ERROR to detect the omission.
* @post On RETURN_OK a profile has been downloaded and is ready for installation, and it
*       appears in a subsequent cellular_esim_get_profile_info() enumeration. This
*       interface does not state the value of eSIMProfileStruct.profileState for a newly
*       downloaded profile, so a caller that needs it active calls
*       cellular_esim_enable_profile() explicitly.
* @post On RETURN_ERROR nothing is reported to the caller beyond the status, and this
*       interface does not state whether a partially downloaded profile can remain on the
*       eUICC; a caller that must know enumerates the card rather than assuming it is
*       unchanged.
*
* @returns int - the outcome of the download.
* @retval RETURN_OK - The profile was downloaded and is ready for installation.
* @retval RETURN_ERROR - The download failed, for example because the address could not
*         be reached, the service advertised nothing for this eUICC, or the eUICC
*         rejected the material. This interface does not distinguish those cases: it
*         defines one failure code and no way to ask which of them occurred. The client
*         treats no profile as downloaded until it has confirmed otherwise by
*         enumeration, and looks to the vendor log named under `Logging and debugging
*         requirements` for the reason. Retrying is a caller decision; no retry policy,
*         count or interval is specified by this interface.
*
* @note This function is documented against RETURN_OK and RETURN_ERROR, unlike
*       cellular_esim_download_profile_with_activationcode(), which is documented against
*       TRUE and FALSE. The comparison that is correct here is not correct there; see the
*       warning on that declaration.
* @note Blocking: the call is synchronous - the outcome is the return value and no part
*       of it arrives later, since this path reports no progress. It contacts a remote
*       SM-DS server, so its duration depends on a network round trip rather than on
*       local computation. Even so, this interface does not specify whether it may block,
*       and `Blocking calls` states that explicitly for all three download paths; a
*       caller must not assume either behaviour. No timeout or completion-time bound is
*       specified anywhere in this repository, so a caller that cannot tolerate an
*       unbounded wait imposes and enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading
*       Model` places on the calling module the obligation to serialise all of its
*       interactions with these APIs, so a caller must not invoke this function
*       concurrently with any other function in this interface. The functions are
*       expected to be callable from several processes at once, so a vendor
*       implementation must also protect its own state against concurrent access from
*       different processes.
* @see cellular_esim_download_profile_with_activationcode
* @see cellular_esim_download_profile_from_defaultsmdp
* @see cellular_esim_get_profile_info
* @see cellular_esim_enable_profile
* @see cellular_esim_lpa_init
*/
int cellular_esim_download_profile_from_smds(char* smds);

/**
* @brief Downloads an eSIM profile from a default SM-DP+ address.
*
* Connects to the Subscription Manager Data Preparation server the caller names - the
* server that holds prepared profiles and delivers them over a secured channel - and
* downloads the profile it holds for this eUICC. It is the third of the three
* alternative download paths in this interface and, like the SM-DS path, reports no
* intermediate progress: it takes no callback argument, so the return value is the only
* outcome a caller sees. It is declared unconditionally, as all three paths are, so a
* caller selects between alternatives rather than testing for a path's presence.
*
* @param[in] smdp SM-DP+ address as a fully qualified name, for example
*    @code "smdp-plus.test.gsma.com" @endcode
*                 The caller allocates the buffer and owns it throughout, and it is the
*                 caller that eventually releases it - though not on the strength of this
*                 call returning, for the reason below; `Memory Model` places both the
*                 allocation and the zero termination on the caller, and the termination
*                 is the only way the length can be determined, since no length
*                 accompanies the pointer. Despite the name of this function, the address
*                 is a parameter rather than a value the implementation supplies: this
*                 interface states no default that applies when the argument is absent,
*                 and no behaviour for a NULL pointer or an empty string. It states no
*                 maximum length and no validation rule beyond the worked value above.
*                 This interface does not specify whether the implementation retains the
*                 pointer beyond the call, and it declares no call through which a
*                 retained pointer could be withdrawn, so the caller keeps ownership but
*                 must not read the return as permission to reuse or release the buffer;
*                 equally, the parameter is declared char* rather than const char*, so
*                 this interface does not establish that the buffer is left unmodified.
*
* @pre cellular_esim_lpa_init() must have completed successfully; `Initialization and
*      Startup` makes it mandatory before any other function in this interface. This
*      interface does not state what this function does when it has not been called, so a
*      caller must not rely on RETURN_ERROR to detect the omission.
* @post On RETURN_OK the profile has been downloaded and appears in a subsequent
*       cellular_esim_get_profile_info() enumeration. This interface does not state the
*       value of eSIMProfileStruct.profileState for a newly downloaded profile, so a
*       caller that needs it active calls cellular_esim_enable_profile() explicitly.
* @post On RETURN_ERROR nothing is reported beyond the status, and this interface does
*       not state whether a partially downloaded profile can remain on the eUICC; a
*       caller that must know enumerates the card rather than assuming it is unchanged.
*
* @returns int - the outcome of the download.
* @retval RETURN_OK - The profile was downloaded successfully.
* @retval RETURN_ERROR - The download failed, for example because the address was
*         malformed or unreachable, the network was unavailable, or the server held no
*         profile for this eUICC. This interface defines one failure code and no way to
*         ask which of those occurred. The client treats no profile as downloaded until
*         it has confirmed otherwise by enumeration, and looks to the vendor log named
*         under `Logging and debugging requirements` for the reason. Retrying is a caller
*         decision; no retry policy, count or interval is specified by this interface.
*
* @note This function is documented against RETURN_OK and RETURN_ERROR, unlike
*       cellular_esim_download_profile_with_activationcode(), which is documented against
*       TRUE and FALSE. The comparison that is correct here is not correct there; see the
*       warning on that declaration.
* @note Blocking: the call is synchronous - the outcome is the return value and no part
*       of it arrives later, since this path reports no progress. It contacts a remote
*       SM-DP+ server, so its duration depends on a network round trip rather than on
*       local computation. Even so, this interface does not specify whether it may block,
*       and `Blocking calls` states that explicitly for all three download paths; a
*       caller must not assume either behaviour. No timeout or completion-time bound is
*       specified anywhere in this repository, so a caller that cannot tolerate an
*       unbounded wait imposes and enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading
*       Model` places on the calling module the obligation to serialise all of its
*       interactions with these APIs, so a caller must not invoke this function
*       concurrently with any other function in this interface. The functions are
*       expected to be callable from several processes at once, so a vendor
*       implementation must also protect its own state against concurrent access from
*       different processes.
* @see cellular_esim_download_profile_with_activationcode
* @see cellular_esim_download_profile_from_smds
* @see cellular_esim_get_profile_info
* @see cellular_esim_enable_profile
* @see cellular_esim_lpa_init
*/
int cellular_esim_download_profile_from_defaultsmdp(char* smdp);

/**
* @brief Enumerates the MNO profiles installed on the eUICC, as a callee-allocated array
*        the caller releases.
*
* Reads the profiles present on the card and reports them as an array of
* eSIMProfileStruct together with the number of entries it contains. This is how a
* caller discovers the ICCID values that cellular_esim_enable_profile(),
* cellular_esim_disable_profile() and cellular_esim_delete_profile() are addressed by,
* so it is the entry point to the profile lifecycle rather than a diagnostic. It is also
* the only function in this interface that allocates on the caller's behalf, which the
* repository specification states under `Memory Model` in the HAL specification.
*
* @param[out] profile_list Address of a caller-held eSIMProfileStruct* through which the
*                          implementation returns the array it allocated. The caller
*                          supplies the address of its own pointer variable - the pointer
*                          variable itself need not be initialised, but its address must
*                          be valid - and on success reads the array through it. The
*                          allocation belongs to the caller from the moment the function
*                          returns success: the caller must release it, and the note
*                          below states what this interface does and does not establish
*                          about how. The number of entries that may be read is the value
*                          returned through nb_profiles and nothing else - the array
*                          carries no terminator and no sentinel entry, so a caller must
*                          not walk it looking for one. This interface does not state
*                          whether the pointer is written on failure, so a caller reads
*                          and releases it only after success, and does not state the
*                          behaviour for a NULL argument.
* @param[out] nb_profiles Address of a caller-held int that receives the number of
*                         entries populated in the array. The caller allocates that int
*                         and keeps it valid for the duration of the call. It bounds every
*                         read of the array. This interface does not state whether a count
*                         of zero is reported as success with an empty array or as a
*                         failure - the existing documentation of the failure code names
*                         "no profiles found" among the failures - so a caller must handle
*                         both and must not treat zero as impossible. It does not state
*                         whether the int is written on failure, nor the behaviour for a
*                         NULL argument.
*
* @pre cellular_esim_lpa_init() must have completed successfully; `Initialization and
*      Startup` makes it mandatory before any other function in this interface.
* @post On RETURN_OK profile_list addresses an array of nb_profiles populated records,
*       and the caller owns that array. Each record is bounded by the field sizes
*       documented on eSIMProfileStruct. The array is a snapshot: this interface does not
*       state that it tracks later changes to the card, so a caller that enables, disables
*       or deletes a profile re-reads rather than reusing it.
* @post On RETURN_ERROR neither output may be read and nothing may be released, because
*       this interface does not establish that either output was written.
*
* @returns int - the outcome of the enumeration.
* @retval RETURN_OK - The profiles were read; the caller may read the array subject to
*         the count, and owns it.
* @retval RETURN_ERROR - The enumeration failed, for example because the eUICC could not
*         be reached, an internal allocation failed, or no profile was found. This
*         interface defines one failure code and no way to ask which of those occurred, so
*         a client cannot distinguish an empty card from a broken one at the call site. It
*         releases nothing, treats the profile set as unknown, and looks to the vendor log
*         named under `Logging and debugging requirements` for the reason.
*
* @note Ownership of the array, stated as far as this interface establishes it and no
*       further: the implementation allocates it and the caller must free it, which is the
*       one caller obligation `Memory Model` places on this interface. This interface does
*       not name the function that releases it and does not state which allocator produced
*       it, so it does not establish that free() is the correct call; a caller that cannot
*       determine the vendor's allocator from its own build has no way to release the array
*       correctly from this interface alone. Nor does this interface state whether the
*       array is invalidated by a subsequent cellular_esim_lpa_exit(), so a caller that
*       cannot tolerate either behaviour releases the array before tearing the module down.
* @note Blocking: the call is synchronous - the outcome is the return value, and nothing
*       about the result arrives later. `Blocking calls` expects every function in this
*       interface to complete in a time commensurate with the complexity of the operation
*       requested, and this one grows with the number of profiles installed on the card,
*       but beyond that expectation this interface does not state whether the call may
*       block and specifies no timeout or upper bound. A caller that cannot tolerate an
*       unbounded wait imposes and enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading
*       Model` places on the calling module the obligation to serialise all of its
*       interactions with these APIs, so a caller must not invoke this function
*       concurrently with any other function in this interface - in particular not
*       concurrently with a download, an enable, a disable or a delete, each of which
*       changes what this function would report. The functions are expected to be callable
*       from several processes at once, so a vendor implementation must also protect its
*       own state against concurrent access from different processes.
* @see eSIMProfileStruct
* @see cellular_esim_enable_profile
* @see cellular_esim_disable_profile
* @see cellular_esim_delete_profile
* @see cellular_esim_lpa_exit
*/
int cellular_esim_get_profile_info(eSIMProfileStruct **profile_list, int *nb_profiles);

/**
* @brief Activates the installed MNO (Mobile Network Operator) profile whose ICCID matches
*        the one supplied.
*
* Matches the supplied ICCID against the profiles stored on the eUICC and activates the
* one it identifies, moving that profile's eSIMProfileStruct.profileState to the enabled
* value 01. The profile must already be installed: this function does not download one,
* and it fails when no stored profile matches. A caller obtains the ICCID values it may
* pass from cellular_esim_get_profile_info(), the only source of them in this interface.
* This interface does not state what becomes of a profile that was already enabled when
* another is enabled, so a caller that depends on only one profile being active must
* establish that behaviour with its vendor.
*
* @param[in] iccid ICCID of the profile to activate, as a 20-digit string in the
*                  format of ITU-T Recommendation E.118. The caller allocates the
*                  buffer and owns it throughout, and it is the caller that eventually
*                  releases it - though not on the strength of this call returning, for
*                  the reason below; `Memory Model` in the HAL specification places
*                  both the
*                  allocation and the zero termination on the caller. This interface does
*                  not specify whether the implementation retains the pointer beyond the
*                  call, and it declares no call through which a retained pointer could be
*                  withdrawn, so the caller keeps ownership but must not read the return
*                  as permission to reuse or release the buffer; equally, the parameter is
*                  declared char* rather than const char*, so this interface does not
*                  establish that the buffer is left unmodified. A value read from
*                  eSIMProfileStruct.iccid sits in a 32-byte field, so a caller must not
*                  pass the field size as the length. The behaviour for a NULL pointer, a
*                  string that is not 20 digits, or a string whose length disagrees with
*                  iccid_size is not specified by this interface.
* @param[in] iccid_size Length of the ICCID string, which this interface documents as 20.
*                       It is the digit count of the value, not the size of the buffer
*                       carrying it and not a count that includes the terminator. This
*                       interface does not state whether an implementation validates the
*                       value, rejects a disagreement with the actual string length, or
*                       accepts any other value, so a caller passes 20 and must not rely
*                       on a length check being performed on its behalf.
*
* @pre cellular_esim_lpa_init() must have completed successfully; `Initialization and
*      Startup` makes it mandatory before any other function in this interface.
* @pre A profile with this ICCID must already be installed on the eUICC, whether by one of
*      the three download paths or before the card reached the caller. `Method Sequencing`
*      states the same ordering.
* @post On RETURN_OK the identified profile is active and reads back with
*       eSIMProfileStruct.profileState 01 on a subsequent
*       cellular_esim_get_profile_info(). Any array read before this call is a stale
*       snapshot and is re-read rather than reused.
* @post On RETURN_ERROR this interface does not establish that the card is unchanged, so a
*       caller that must know re-reads the card rather than assuming the previous profile
*       state survived.
*
* @returns int - the outcome of the operation.
* @retval RETURN_OK - The profile was enabled.
* @retval RETURN_ERROR - The operation failed, for example because no stored profile
*         matched the ICCID, the value was not in a valid format, or the implementation hit
*         an internal error. This interface defines one failure code and no way to ask
*         which of those occurred. The client re-reads the profile list to establish which
*         profiles exist and what state they are in, corrects the ICCID at the call site if
*         it was wrong - retrying with the same unmatched value cannot succeed - and looks
*         to the vendor log named under `Logging and debugging requirements` for the
*         reason.
*
* @note Blocking: the call is synchronous - the outcome is the return value, and nothing
*       about the result arrives later. `Blocking calls` expects it to complete in a time
*       commensurate with the complexity of the operation requested, but beyond that
*       expectation this interface does not state whether it may block and specifies no
*       timeout or upper bound. A caller that cannot tolerate an unbounded wait imposes and
*       enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading Model`
*       places on the calling module the obligation to serialise all of its interactions
*       with these APIs, so a caller must not invoke this function concurrently with any
*       other function in this interface, and in particular not concurrently with a
*       disable or a delete addressed at the same profile. The functions are expected to be
*       callable from several processes at once, so a vendor implementation must also
*       protect its own state against concurrent access from different processes.
* @see cellular_esim_disable_profile
* @see cellular_esim_delete_profile
* @see cellular_esim_get_profile_info
* @see eSIMProfileStruct
*/
int cellular_esim_enable_profile(char* iccid, int iccid_size);

/**
* @brief Deactivates the installed eSIM profile whose ICCID matches the one supplied,
*        leaving it installed.
*
* Matches the supplied ICCID exactly against the profiles stored on the eUICC and
* deactivates the one it identifies, moving that profile's
* eSIMProfileStruct.profileState to the disabled value 00. The profile remains installed
* and continues to appear in cellular_esim_get_profile_info(); removing it is
* cellular_esim_delete_profile(), which is a different and irreversible operation. This
* interface does not state what happens when the profile named is already disabled, nor
* whether disabling the active profile leaves the device with no active subscription -
* though nothing here activates another profile in its place.
*
* @param[in] iccid ICCID of the profile to deactivate, as a 20-digit string in the
*                  format of ITU-T Recommendation E.118, matched exactly.
*                  The caller allocates the buffer and owns it throughout, and it is the
*                  caller that eventually releases it - though not on the strength of this
*                  call returning, for the reason below; `Memory Model`
*                  in the HAL specification places both the allocation and the zero
*                  termination on the caller. This interface does not specify whether the
*                  implementation retains the pointer beyond the call, and it declares no
*                  call through which a retained pointer could be withdrawn, so the caller
*                  keeps ownership but must not read the return as permission to reuse or
*                  release the buffer; equally, the parameter is declared char* rather
*                  than const
*                  char*, so this interface does not establish that the buffer is left
*                  unmodified. A value read from eSIMProfileStruct.iccid sits in a 32-byte
*                  field, so a caller must not pass the field size as the length. The
*                  behaviour for a NULL pointer, a string that is not 20 digits, or a
*                  string whose length disagrees with iccid_size is not specified by this
*                  interface.
* @param[in] iccid_size Length of the ICCID string, which this interface documents as 20.
*                       It is the digit count of the value, not the size of the buffer
*                       carrying it and not a count that includes the terminator. This
*                       interface does not state whether an implementation validates the
*                       value or rejects a disagreement with the actual string length, so a
*                       caller passes 20 and must not rely on a length check being
*                       performed on its behalf.
*
* @pre cellular_esim_lpa_init() must have completed successfully; `Initialization and
*      Startup` makes it mandatory before any other function in this interface.
* @pre A profile with this ICCID must already be installed on the eUICC; `Method
*      Sequencing` states the same ordering.
* @post On RETURN_OK the identified profile is inactive and reads back with
*       eSIMProfileStruct.profileState 00 on a subsequent
*       cellular_esim_get_profile_info(). The profile is still installed. Any array read
*       before this call is a stale snapshot and is re-read rather than reused.
* @post On RETURN_ERROR this interface does not establish that the card is unchanged, so a
*       caller that must know re-reads it rather than assuming the previous profile state
*       survived.
*
* @returns int - the outcome of the operation.
* @retval RETURN_OK - The profile was disabled.
* @retval RETURN_ERROR - The operation failed, for example because no stored profile
*         matched the ICCID, the value was not in a valid format, or the implementation hit
*         an internal error. This interface defines one failure code and no way to ask
*         which of those occurred. The client re-reads the profile list to establish which
*         profiles exist and what state they are in, corrects the ICCID at the call site if
*         it was wrong - retrying with the same unmatched value cannot succeed - and looks
*         to the vendor log named under `Logging and debugging requirements` for the
*         reason.
*
* @note Blocking: the call is synchronous - the outcome is the return value, and nothing
*       about the result arrives later. `Blocking calls` expects it to complete in a time
*       commensurate with the complexity of the operation requested, but beyond that
*       expectation this interface does not state whether it may block and specifies no
*       timeout or upper bound. A caller that cannot tolerate an unbounded wait imposes and
*       enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading Model`
*       places on the calling module the obligation to serialise all of its interactions
*       with these APIs, so a caller must not invoke this function concurrently with any
*       other function in this interface, and in particular not concurrently with an enable
*       or a delete addressed at the same profile. The functions are expected to be
*       callable from several processes at once, so a vendor implementation must also
*       protect its own state against concurrent access from different processes.
* @see cellular_esim_enable_profile
* @see cellular_esim_delete_profile
* @see cellular_esim_get_profile_info
* @see eSIMProfileStruct
*/
int cellular_esim_disable_profile(char* iccid, int iccid_size);

/**
* @brief Permanently removes the installed eSIM profile whose ICCID matches the one
*        supplied.
*
* Matches the supplied ICCID exactly against the profiles stored on the eUICC and deletes
* the one it identifies from the card. The removal is permanent and this interface offers
* no undo: a deleted profile is no longer reported by cellular_esim_get_profile_info(),
* and recovering it means downloading it again through one of the three download paths,
* which requires whatever credential that path needs. Profile state lives on the card
* rather than in the module, so deletion outlives the lifetime of the HAL module; the
* repository specification states that under `Persistence Model`
* in the HAL specification. This interface does not state whether an enabled profile may
* be deleted while it is active, nor what becomes of the device's subscription if it is.
*
* @param[in] iccid ICCID of the profile to remove, as a 20-digit string in the format
*                  of ITU-T Recommendation E.118, matched exactly. The caller
*                  allocates the buffer and owns it throughout, and it is the caller
*                  that eventually releases it - though not on the strength of this call
*                  returning, for the reason below; `Memory Model` places both the
*                  allocation and
*                  the zero termination on the caller. This interface does not specify
*                  whether the implementation retains the pointer beyond the call, and it
*                  declares no call through which a retained pointer could be withdrawn,
*                  so the caller keeps ownership but must not read the return as
*                  permission to reuse or release the buffer; equally, the parameter is
*                  declared char* rather than const
*                  char*, so this interface does not establish that the buffer is left
*                  unmodified. A value read from eSIMProfileStruct.iccid sits in a 32-byte
*                  field, so a caller must not pass the field size as the length. Because
*                  the effect is irreversible, a caller should take the value from a
*                  freshly read profile list rather than from a cached one. The behaviour
*                  for a NULL pointer, a string that is not 20 digits, or a string whose
*                  length disagrees with iccid_size is not specified by this interface.
* @param[in] iccid_size Length of the ICCID string, which this interface documents as 20.
*                       It is the digit count of the value, not the size of the buffer
*                       carrying it and not a count that includes the terminator. This
*                       interface does not state whether an implementation validates the
*                       value or rejects a disagreement with the actual string length, so a
*                       caller passes 20 and must not rely on a length check being
*                       performed on its behalf.
*
* @pre cellular_esim_lpa_init() must have completed successfully; `Initialization and
*      Startup` makes it mandatory before any other function in this interface.
* @pre A profile with this ICCID must already be installed on the eUICC; `Method
*      Sequencing` states the same ordering.
* @post On RETURN_OK the profile has been removed from the eUICC and no longer appears in
*       cellular_esim_get_profile_info(). Any array read before this call is a stale
*       snapshot and is re-read rather than reused. The removal survives
*       cellular_esim_lpa_exit() and any later bring-up, because the profile lived on the
*       card.
* @post On RETURN_ERROR this interface does not establish that the card is unchanged, so a
*       caller that must know whether the profile survived re-reads the card rather than
*       assuming it did.
*
* @returns int - the outcome of the operation.
* @retval RETURN_OK - The profile was deleted.
* @retval RETURN_ERROR - The operation failed, for example because no stored profile
*         matched the ICCID, the value was not in a valid format, or the implementation hit
*         an internal error. This interface defines one failure code and no way to ask
*         which of those occurred, so a client cannot tell a refusal from a missing profile
*         at the call site. It re-reads the profile list to establish what is still
*         installed, corrects the ICCID at the call site if it was wrong - retrying with
*         the same unmatched value cannot succeed - and looks to the vendor log named under
*         `Logging and debugging requirements` for the reason.
*
* @warning The effect is irreversible through this interface. Deleting the wrong profile
*          cannot be undone here, and the only recovery is a fresh download of that
*          profile.
* @note Blocking: the call is synchronous - the outcome is the return value, and nothing
*       about the result arrives later. `Blocking calls` expects it to complete in a time
*       commensurate with the complexity of the operation requested, but beyond that
*       expectation this interface does not state whether it may block and specifies no
*       timeout or upper bound. A caller that cannot tolerate an unbounded wait imposes and
*       enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading Model`
*       places on the calling module the obligation to serialise all of its interactions
*       with these APIs, so a caller must not invoke this function concurrently with any
*       other function in this interface, and in particular not concurrently with an
*       enable, a disable or an enumeration. The functions are expected to be callable from
*       several processes at once, so a vendor implementation must also protect its own
*       state against concurrent access from different processes - which matters more here
*       than anywhere else in this interface, because the operation destroys state.
* @see cellular_esim_enable_profile
* @see cellular_esim_disable_profile
* @see cellular_esim_get_profile_info
* @see cellular_esim_download_profile_with_activationcode
*/
int cellular_esim_delete_profile(char* iccid, int iccid_size);

/**
* @brief Brings the Local Profile Assistant up, and must be called before any other
*        function in this interface.
*
* Configures the LPA module, establishes that the modem is ready and sets up the
* dependencies and parameters the eSIM operations need. It is mandatory: the repository
* specification states under `Initialization and Startup` in the HAL specification that
* it must precede every other function here, including the two identity reads, and that a
* vendor implementation must check that the eUICC and the modem are present and functional
* so that later operations do not fail against unprepared hardware.
*
* The call returns a status rather than a handle or a context pointer, so the module state
* is implicit: a caller cannot address more than one instance of it through this interface
* and cannot hand an instance to another component. This interface does not state whether
* calling this function twice without an intervening cellular_esim_lpa_exit() is
* permitted, nor whether a second call is idempotent, so a caller must not rely on either.
*
* @pre This interface states no pre-condition for this call. It takes no argument, so
*      there is no caller-supplied value to be valid, and nothing here establishes an
*      external API dependency that must be satisfied first.
* @post On RETURN_OK the module is ready and every other function in this interface may be
*       called. On RETURN_ERROR the module must be treated as unavailable: this interface
*       does not establish that a partially initialized module is usable, and it does not
*       state whether cellular_esim_lpa_exit() must be called after a failed
*       initialization, so a caller that wants to release anything the attempt may have
*       acquired should establish that with its vendor.
*
* @returns int - the outcome of the initialization.
* @retval RETURN_OK - The Local Profile Assistant was initialized and the interface is
*         ready for use.
* @retval RETURN_ERROR - Initialization failed, for example because the modem was
*         unavailable or misconfigured, the eUICC was absent, or the vendor's LPA software
*         reported an error. This interface defines one failure code and no way to ask
*         which of those occurred. The client treats the whole interface as unavailable,
*         makes no further call into it, and looks to the vendor log named under `Logging
*         and debugging requirements` for the reason. Whether a retry can succeed depends
*         on a cause this interface does not report, so no retry policy, count or interval
*         is specified here.
*
* @note Blocking: this interface does not specify whether this call blocks, and it
*       specifies no completion-time bound. **A caller must not assume either
*       behaviour**, must not depend on this call returning promptly at bring-up, and
*       imposes and enforces its own bound where one is required.
* @note Thread safety: this interface is not required to be thread safe. `Threading Model`
*       places on the calling module the obligation to serialise all of its interactions
*       with these APIs, so this call must complete before any other call into this
*       interface is made, and it must not be invoked concurrently with one. The functions
*       are expected to be callable from several processes at once, so a vendor
*       implementation must also protect its own state against concurrent access from
*       different processes, including two processes initializing at the same time.
* @see cellular_esim_lpa_exit
* @see cellular_esim_get_profile_info
* @see cellular_esim_download_profile_with_activationcode
*/
int cellular_esim_lpa_init(void);

/**
* @brief Tears the Local Profile Assistant down and releases the resources it holds.
*
* Deactivates the module and releases what the implementation allocated internally,
* including memory, file handles and any threads it started; `Threading Model` and `Memory
* Model` in the HAL specification place that cleanup obligation on the implementation. A
* caller invokes it before shutdown, or once eSIM functionality is no longer needed, and it
* is the last call a caller makes into this interface.
*
* What it does not do matters as much as what it does. Installed profiles live on the eUICC
* and are unaffected: `Persistence Model` states that profile state outlives any single
* lifetime of the module, so tear-down neither disables nor removes a profile, and only
* cellular_esim_delete_profile() removes one. This interface also does not state whether
* the array returned by an earlier cellular_esim_get_profile_info() is invalidated here, so
* a caller that cannot tolerate either behaviour releases that array before calling this
* function.
*
* @pre cellular_esim_lpa_init() must have completed successfully; this interface does not
*      state what this function does when it has not, and does not state whether it may be
*      called after a failed initialization.
* @pre No other call into this interface may be in progress. This interface does not state
*      what happens to an operation still running - the existing documentation of the
*      failure code names ongoing operations among the causes of failure - so a caller lets
*      every outstanding call return first.
* @post On RETURN_OK the module is torn down and no further call into this interface may be
*       made until cellular_esim_lpa_init() has succeeded again. Installed profiles are
*       unchanged.
* @post On RETURN_ERROR this interface does not establish whether the module is up, down or
*       partly torn down, and does not state whether the call may be repeated; a caller
*       that must reach a known state re-initializes rather than assuming either outcome.
*
* @returns int - the outcome of the tear-down.
* @retval RETURN_OK - The Local Profile Assistant was deactivated and its resources
*         released.
* @retval RETURN_ERROR - Tear-down failed, for example because a resource could not be
*         released, an operation was still in progress, or the vendor's LPA software
*         reported an error. This interface defines one failure code and no way to ask which
*         of those occurred. The client makes no further call into the interface, treats any
*         resource the implementation held as possibly still held - it has no way to release
*         it from this interface - and looks to the vendor log named under `Logging and
*         debugging requirements` for the reason.
*
* @note Blocking: the call is synchronous - the outcome is the return value, and nothing
*       about the result arrives later. `Blocking calls` expects it to complete in a time
*       commensurate with the complexity of the operation requested, but beyond that
*       expectation this interface does not state whether it may block, for instance while
*       waiting for an internal thread to join, and specifies no timeout or upper bound. A
*       caller tearing down on a shutdown path that cannot tolerate an unbounded wait
*       imposes and enforces its own bound.
* @note Thread safety: this interface is not required to be thread safe. `Threading Model`
*       places on the calling module the obligation to serialise all of its interactions
*       with these APIs, and that obligation is at its strictest here: this call must not
*       be invoked concurrently with any other function in this interface, because it
*       releases the state those functions use. The functions are expected to be callable
*       from several processes at once, and this interface does not state how a
*       multi-process caller determines that no other process still needs the module, so
*       that coordination is the caller's to arrange.
* @see cellular_esim_lpa_init
* @see cellular_esim_get_profile_info
* @see cellular_esim_delete_profile
*/
int cellular_esim_lpa_exit(void);

/**
* @brief Reads the EID (eUICC Identifier) of the device's eSIM, and reports only whether
*        the read succeeded.
*
* Instructs the implementation to fetch the unique identifier of the embedded card - the
* value that identifies the eUICC itself rather than any profile on it. The return value
* reports whether the read succeeded and carries nothing else.
*
* @pre cellular_esim_lpa_init() must have completed successfully. `Initialization and
*      Startup` makes it mandatory before any other function in this interface, and
*      `Method Sequencing` states that the two identity reads are not exempt, so a caller
*      must not treat this as callable beforehand.
* @post On RETURN_OK the implementation has read the EID. Where the value can be observed
*       is not established by this interface; see the warning below.
* @post On RETURN_ERROR nothing is reported to the caller beyond the status.
*
* @returns int - the outcome of the read. It is a status only: the EID itself is not
*          returned through it, and this declaration has no output parameter to return it
*          through.
* @retval RETURN_OK - The EID was read successfully.
* @retval RETURN_ERROR - The read failed, for example because the eUICC could not be
*         communicated with, no eUICC is present, or the implementation is misconfigured.
*         This interface defines one failure code and no way to ask which of those
*         occurred. The client treats the EID as unavailable and looks to the vendor log
*         named under `Logging and debugging requirements` for the reason.
*
* @warning This function is declared with no parameter, so it has no output buffer and no
*          out-parameter, and its int return is documented as a status code. **This
*          interface therefore does not establish how the EID reaches the caller.** A
*          caller cannot determine from this interface where to read the value, and a test
*          cannot assert on a value the interface gives it no way to obtain. The repository
*          specification records the same gap under `API Surface`. Anything a particular
*          vendor does instead - a side channel, a value packed into the return, a
*          companion call outside this interface - is a vendor arrangement and is not part
*          of this contract.
* @warning The declaration uses an empty parameter list rather than (void), which is not
*          the same declaration in C: an empty list does not prototype the parameters, so a
*          C compiler cannot check a call against it and a call passing arguments is not
*          rejected. C++ treats the two forms alike. A caller passes no argument.
*          cellular_esim_lpa_init() and cellular_esim_lpa_exit(), by contrast, are declared
*          (void).
* @note Blocking: the call is synchronous - the outcome is the return value, and nothing
*       about the result arrives later. `Blocking calls` expects it to complete in a time
*       commensurate with the complexity of the operation requested, but beyond that
*       expectation this interface does not state whether it may block while communicating
*       with the card, and specifies no timeout or upper bound. A caller that cannot
*       tolerate an unbounded wait imposes and enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading Model`
*       places on the calling module the obligation to serialise all of its interactions
*       with these APIs, so a caller must not invoke this function concurrently with any
*       other function in this interface. The functions are expected to be callable from
*       several processes at once, so a vendor implementation must also protect its own
*       state against concurrent access from different processes.
* @see cellular_esim_get_euicc
* @see cellular_esim_lpa_init
*/
int cellular_esim_get_eid();

/**
* @brief Reads the information the eUICC reports about itself, and reports only whether
*        the read succeeded.
*
* Instructs the implementation to fetch the details the embedded Universal Integrated
* Circuit Card holds about itself - the card-level information a caller uses when managing
* subscriptions, as distinct from the per-profile records
* cellular_esim_get_profile_info() reports and from the identifier
* cellular_esim_get_eid() reads. The return value reports whether the read succeeded and
* carries nothing else.
*
* @pre cellular_esim_lpa_init() must have completed successfully. `Initialization and
*      Startup` makes it mandatory before any other function in this interface, and
*      `Method Sequencing` states that the two identity reads are not exempt.
* @post On RETURN_OK the implementation has read the eUICC information. Where the values
*       can be observed is not established by this interface; see the warning below.
* @post On RETURN_ERROR nothing is reported to the caller beyond the status.
*
* @returns int - the outcome of the read. It is a status only: the eUICC information is not
*          returned through it, and this declaration has no output parameter to return it
*          through.
* @retval RETURN_OK - The eUICC information was read successfully.
* @retval RETURN_ERROR - The read failed, for example because the eUICC could not be
*         accessed, communication with it failed, or the implementation is misconfigured.
*         This interface defines one failure code and no way to ask which of those
*         occurred. The client treats the information as unavailable and looks to the vendor
*         log named under `Logging and debugging requirements` for the reason.
*
* @warning This function is declared with no parameter, so it has no output buffer and no
*          out-parameter, and its int return is documented as a status code. **This
*          interface therefore does not establish how the eUICC information reaches the
*          caller**, nor which fields that information consists of: no structure in this
*          header describes it, and eSIMProfileStruct describes a profile rather than the
*          card. A caller cannot determine from this interface where to read the values or
*          what they are, and a test cannot assert on them. The repository specification
*          records the same gap under `API Surface`.
* @warning The declaration uses an empty parameter list rather than (void), which is not
*          the same declaration in C: an empty list does not prototype the parameters, so a
*          C compiler cannot check a call against it and a call passing arguments is not
*          rejected. C++ treats the two forms alike. A caller passes no argument.
* @note Blocking: the call is synchronous - the outcome is the return value, and nothing
*       about the result arrives later. `Blocking calls` expects it to complete in a time
*       commensurate with the complexity of the operation requested, but beyond that
*       expectation this interface does not state whether it may block while communicating
*       with the card, and specifies no timeout or upper bound. A caller that cannot
*       tolerate an unbounded wait imposes and enforces its own.
* @note Thread safety: this interface is not required to be thread safe. `Threading Model`
*       places on the calling module the obligation to serialise all of its interactions
*       with these APIs, so a caller must not invoke this function concurrently with any
*       other function in this interface. The functions are expected to be callable from
*       several processes at once, so a vendor implementation must also protect its own
*       state against concurrent access from different processes.
* @see cellular_esim_get_eid
* @see cellular_esim_get_profile_info
* @see cellular_esim_lpa_init
*/
int cellular_esim_get_euicc();
#endif //_LPA_HAL_H_

/**
* @}
*/
