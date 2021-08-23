/**
 * @file linux_adu_core_exports.cpp
 * @brief Implements exported methods for platform-specific ADUC agent code.
 *
 * @copyright Copyright (c) 2019, Microsoft Corporation.
 */
#include "aduc/adu_core_exports.h"
#include "aduc/c_utils.h"
#include "aduc/logging.h"
#include "aduc/process_utils.hpp"
#include "linux_adu_core_impl.hpp"
#include <memory>
#include <signal.h> // raise()
#include <string>
#include <sys/reboot.h> // reboot()
#include <time.h>
#include <unistd.h> // sync()
#include <vector>
EXTERN_C_BEGIN

/**
 * @brief Register this module for callbacks.
 *
 * @param data Information about this module (e.g. callback methods)
 * @return ADUC_Result Result code.
 */
ADUC_Result ADUC_Register(ADUC_RegisterData* data, unsigned int /*argc*/, const char** /*argv*/)
{
    try
    {
        std::unique_ptr<ADUC::LinuxPlatformLayer> pImpl{ ADUC::LinuxPlatformLayer::Create() };
        ADUC_Result result{ pImpl->SetRegisterData(data) };
        // The platform layer object is now owned by the RegisterData object.
        pImpl.release();
        return result;
    }
    catch (const ADUC::Exception& e)
    {
        Log_Error("Unhandled ADU Agent exception. code: %d, message: %s", e.Code(), e.Message().c_str());
        return ADUC_Result{ ADUC_RegisterResult_Failure, e.Code() };
    }
    catch (const std::exception& e)
    {
        Log_Error("Unhandled std exception: %s", e.what());
        return ADUC_Result{ ADUC_RegisterResult_Failure, ADUC_ERC_NOTRECOVERABLE };
    }
    catch (...)
    {
        return ADUC_Result{ ADUC_RegisterResult_Failure, ADUC_ERC_NOTRECOVERABLE };
    }
}

/**
 * @brief Unregister this module.
 *
 * @param token Token that was returned from #ADUC_Register call.
 */
void ADUC_Unregister(ADUC_Token token)
{
    ADUC::LinuxPlatformLayer* pImpl{ static_cast<ADUC::LinuxPlatformLayer*>(token) };
    delete pImpl; // NOLINT(cppcoreguidelines-owning-memory)
}

/**
 * @brief Give the user time to abbort reboot
 * Not in use at the moment
 */

void delay(void)
{
    long akt_zeit;
    long end_zeit;

    int delayTime = 3;

    while (delayTime != 0)
    {
        Log_Info("Time till reboot %d", delayTime);
        time(&end_zeit);
        end_zeit += 1;
        do
        {
            time(&akt_zeit);
        } while (akt_zeit < end_zeit);
        {
            delayTime--;
        }
    }
}

/**
 * @brief Reboot the system.
 *
 * @returns int errno, 0 if success.
 */
int ADUC_RebootSystem()
{
    Log_Info("ADUC_RebootSystem called. Rebooting system.");

    // Commit buffer cache to disk.
    sync();

    std::string output;
    std::vector<std::string> args{ "--reboot", "--no-wall" };
    //delay();
    int exitStatus = ADUC_LaunchChildProcess("/sbin/reboot", args, output);

    if (exitStatus != 0)
    {
        Log_Error("Reboot failed.");
    }

    if (!output.empty())
    {
        Log_Info(output.c_str());
    }

    return exitStatus;
}

/**
 * @brief Restart the ADU Agent.
 *
 * @returns int errno, 0 if success.
 */
int ADUC_RestartAgent()
{
    Log_Info("Restarting ADU Agent.");

    // Commit buffer cache to disk.
    sync();

    // Using SGIUSR1 to indicates a desire for shutdown and restart.
    const int exitStatus = raise(SIGUSR1);
    if (exitStatus != 0)
    {
        Log_Error("ADU Agent restart failed.");
    }

    return exitStatus;
}

EXTERN_C_END
