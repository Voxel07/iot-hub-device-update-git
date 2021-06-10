/**
 * @file swupdate_handler.cpp
 * @brief Implementation of ContentHandler API for swupdate.
 *
 * Will call into wrapper script for swupdate to install image files.
 *
 * microsoft/swupdate
 * v1:
 *   Description:
 *   Initial revision.
 *
 *   Expected files:
 *   .swu - contains swupdate image.
 *
 * @copyright Copyright (c) 2019, Microsoft Corporation.
 */
#include "aduc/fsupdate_handler.hpp"

#include "aduc/adu_core_exports.h"
#include "aduc/logging.h"
#include "aduc/process_utils.hpp"
#include "aduc/string_utils.hpp"
#include "aduc/system_utils.h"
#include "adushell_const.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <iostream>

#include <dirent.h>

namespace adushconst = Adu::Shell::Const;

/**
 * @brief handler creation function
 * This function calls  CreateContentHandler from handler factory 
 */
std::unique_ptr<ContentHandler> microsoft_swupdate_CreateFunc(const ContentHandlerCreateData& data)
{
    Log_Info("microsoft_swupdate_CreateFunc called.");
    return std::unique_ptr<ContentHandler>{ FSUpdateHandlerImpl::CreateContentHandler(
        data.WorkFolder(), data.LogFolder(), data.Filename(), data.FileType()) };
}

// Forward declarations.
static ADUC_Result CancelApply(const char* logFolder);

/**
 * @brief Creates a new FSUpdateHandlerImpl object and casts to a ContentHandler.
 * Note that there is no way to create a FSUpdateHandlerImpl directly.
 *
 * @param workFolder The folder where content will be downloaded.
 * @param logFolder The folder where operational logs can be placed.
 * @param filename The .swu image file to be installed by swupdate.
 * @return std::unique_ptr<ContentHandler> SimulatorHandlerImpl object as a ContentHandler.
 */
std::unique_ptr<ContentHandler> FSUpdateHandlerImpl::CreateContentHandler(
    const std::string& workFolder, const std::string& logFolder, const std::string& filename, const std::string& fileType)
{
    return std::unique_ptr<ContentHandler>{ new FSUpdateHandlerImpl(workFolder, logFolder, filename, fileType) };
}

/**
 * @brief Validate meta data including file count and handler version.
 * @param prepareInfo.
 * @return bool
 */
ADUC_Result FSUpdateHandlerImpl::Prepare(const ADUC_PrepareInfo* prepareInfo)
{
    if (prepareInfo->updateTypeVersion != 1 || prepareInfo->updateTypeVersion != 2)
    {
        Log_Error("FsUpdate packages prepare failed. Wrong Handler Version %d. Set '1' for ff and '2' for af", prepareInfo->updateTypeVersion);
        return ADUC_Result{ ADUC_PrepareResult_Failure,
                            ADUC_ERC_SWUPDATE_HANDLER_PACKAGE_PREPARE_FAILURE_WRONG_VERSION };
    }

    if (prepareInfo->fileCount != 1)
    {
        Log_Error("FsUpdate packages prepare failed. Wrong File Count %d", prepareInfo->fileCount);
        return ADUC_Result{ ADUC_PrepareResult_Failure,
                            ADUC_ERC_SWUPDATE_HANDLER_PACKAGE_PREPARE_FAILURE_WRONG_FILECOUNT };
    }

    Log_Info("Prepare succeeded.");
    return ADUC_Result{ ADUC_PrepareResult_Success };
}

/**
 * @brief Download implementation for swupdate (no-op)
 * swupdate does not need to download additional content.
 * This method is a no-op.
 *
 * @return ADUC_Result The result of the download (always success)
 */
ADUC_Result FSUpdateHandlerImpl::Download()
{
    _isApply = false;
    Log_Info("Download called - no-op for fsupdate");
    return ADUC_Result{ ADUC_DownloadResult_Success };
}

/**
 * @brief Install implementation for swupdate.
 * Calls into the swupdate wrapper script to install an image file.
 *
 * @return ADUC_Result The result of the install.
 */
ADUC_Result FSUpdateHandlerImpl::Install()
{
    _isApply = false;
    Log_Info("Installing from %s", _workFolder.c_str());

    std::unique_ptr<DIR, std::function<int(DIR*)>> directory(
        opendir(_workFolder.c_str()), [](DIR* dirp) -> int { return closedir(dirp); });
    if (directory == nullptr)
    {
        Log_Error("opendir failed, errno = %d", errno);

        return ADUC_Result{ ADUC_InstallResult_Failure, MAKE_ADUC_ERRNO_EXTENDEDRESULTCODE(errno) };
    }

    // Calling the install script to install the update. The script takes the name of the image file as input.
    // Only one file is expected in the work folder. If multiple files exist, treat it as a failure.
    // TODO(Nox): Forcing updates to be a single file is a temporary workaround.
    // We need to have the ability to determine which file is the image file.
    // Then we can pass the appropriate file to the install script.
    const char* filename = nullptr;
    const dirent* entry = nullptr;
    while ((entry = readdir(directory.get())) != nullptr)
    {
        if (entry->d_type == DT_REG)
        {
            if (filename == nullptr)
            {
                filename = entry->d_name;
            }
            else
            {
                Log_Error("More than one file in work folder");
                return ADUC_Result{ ADUC_InstallResult_Failure, ADUC_ERC_NOTPERMITTED };
            }
        }
    }

    if (filename == nullptr)
    {
        Log_Error("No file in work folder");
        return ADUC_Result{ ADUC_InstallResult_Failure, ADUC_ERC_NOTRECOVERABLE };
    }

    if (_filename != filename)
    {
        Log_Warn("Specified filename %s does not match actual filename %s.", _filename.c_str(), filename);
    }

    Log_Info("Installing image file: %s", filename);

    std::string command = adushconst::path_to_fs_update;
    std::vector<std::string> args{ };
   
    if( _fileType.c_str() == "af"){
        args.emplace_back(adushconst::rauc_af_update);
    }
    else if(_fileType.c_str() == "ff"){
        args.emplace_back(adushconst::rauc_ff_update);
    }
    else{
        Log_Error("Invaliede Update Type");
        return ADUC_Result{ ADUC_InstallResult_Failure }; 
    }

    std::stringstream data;
    data << _workFolder << "/" << filename;
    args.emplace_back(data.str().c_str());
    args.emplace_back(adushconst::rauc_debug_mode);
    std::string output;

    Log_Info("---TMP---");
    const int exitCode = ADUC_LaunchChildProcess(command, args, output);

    if (exitCode != 1)
    {
        Log_Error("Install failed, extendedResultCode = %d", exitCode);
        return ADUC_Result{ ADUC_InstallResult_Failure, exitCode };
    }

    Log_Info("Install succeeded");
    return ADUC_Result{ ADUC_InstallResult_Success };
}

/**
 * @brief Apply implementation for FS-Update
 * Calls into the FS-Update wrapper script to perform apply.
 * Will validate a successfull reboot and
 * flip U-Boot flags update and update-reboot-state to 0 
 * to mark the newly bootet partition as good and the olde on as inactive
 *
 * @return ADUC_Result The result of the apply.
 */
ADUC_Result FSUpdateHandlerImpl::Apply()
{
    Log_Info("Apply action called");
    _isApply = true;
   
    std::string command = adushconst::path_to_fs_update;
    std::vector<std::string> args{ adushconst::rauc_commit_update, adushconst::rauc_debug_mode};

    std::string output;

    const int exitCode = ADUC_LaunchChildProcess(command, args, output);

    if (exitCode != 1)
    {
        Log_Error("Apply failed, extendedResultCode = %d", exitCode);
        return ADUC_Result{ ADUC_ApplyResult_Failure, exitCode };
    }

    /**
     * Update adu-version file in /etc/
    */  
    if (!FSUpdateHandlerImpl::UpdateVersionFile("1.0","/etc/adu-version")) 
    {
        return ADUC_Result{ ADUC_ApplyResult_Failure };
    }

    return ADUC_Result{ ADUC_ApplyResult_Success };
 
}

bool FSUpdateHandlerImpl::UpdateVersionFile(const std::string& newVersion ,const std::string& filePath){
   
    if (filePath.empty())
    {
        Log_Error("Empty file path.");
        return false;
    }
    if ((filePath.length()) + 1 > PATH_MAX)
    {
        Log_Error("Path is too long.");
        return false;
    }

    Log_Info("Updating version file from %s to %s",FSUpdateHandlerImpl::ReadValueFromFile(filePath).c_str(),newVersion.c_str());

    std::ofstream ofs;
    ofs.open(filePath, std::ofstream::trunc);
    if(!ofs.is_open())
    {
        Log_Error("File %s failed to open, error: %d", filePath.c_str(), errno);
    }

    ofs << newVersion;
    
    ofs.close();

    return true;
}

/**
 * @brief Cancel implementation for swupdate.
 * We don't have many hooks into swupdate to cancel an ongoing install.
 * We can cancel apply by reverting the bootloader flag to boot into the original partition.
 * Calls into the swupdate wrapper script to cancel apply.
 * Cancel after or during any other operation is a no-op.
 *
 * @return ADUC_Result The result of the cancel.
 */
ADUC_Result FSUpdateHandlerImpl::Cancel()
{
    if (_isApply)
    {
        // swupdate handler can only cancel apply.
        // all other cancels are no-ops.
        return CancelApply(_logFolder.c_str());
    }

    return ADUC_Result{ ADUC_CancelResult_Success };
}

/**
 * @brief Reads a first line of a file, trims trailing whitespace, and returns as string.
 *
 * @param filePath Path to the file to read value from.
 * @return std::string Returns the value from the file. Returns empty string if there was an error.
 */
/*static*/
std::string FSUpdateHandlerImpl::ReadValueFromFile(const std::string& filePath)
{
    Log_Info("---TMP--- ReadValueFromFile");

    if (filePath.empty())
    {
        Log_Error("Empty file path.");
        return std::string{};
    }

    if ((filePath.length()) + 1 > PATH_MAX)
    {
        Log_Error("Path is too long.");
        return std::string{};
    }

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        Log_Error("File %s failed to open, error: %d", filePath.c_str(), errno);
        return std::string{};
    }

    std::string result;
    std::getline(file, result);
    if (file.bad())
    {
        Log_Error("Unable to read from file %s, error: %d", filePath.c_str(), errno);
        return std::string{};
    }

    // Trim whitespace
    ADUC::StringUtils::Trim(result);
    return result;
}

/**
 * @brief Checks if the installed content matches the installed criteria.
 *
 * @param installedCriteria The installed criteria string. e.g. The firmware version.
 *  installedCriteria has already been checked to be non-empty before this call.
 *
 * @return ADUC_Result
 */
ADUC_Result FSUpdateHandlerImpl::IsInstalled(const std::string& installedCriteria)
{
    Log_Info("---TMP---IsInstalled call reading from File");
    std::string version{ ReadValueFromFile(ADUC_VERSION_FILE) };
    if (version.empty())
    {
        Log_Error("Version file %s did not contain a version or could not be read.", ADUC_VERSION_FILE);
        return ADUC_Result{ ADUC_IsInstalledResult_Failure };
    }

    if (version == installedCriteria)
    {
        Log_Info("Installed criteria %s was installed.", installedCriteria.c_str());
        return ADUC_Result{ ADUC_IsInstalledResult_Installed };
    }

    Log_Info(
        "Installed criteria %s was not installed, the current version is %s",
        installedCriteria.c_str(),
        version.c_str());
    return ADUC_Result{ ADUC_IsInstalledResult_NotInstalled };
}

/**
 * @brief Helper function to perform cancel when we are doing an apply.
 *
 * @return ADUC_Result The result of the cancel.
 */
static ADUC_Result CancelApply(const char* logFolder)
{
    // Execute the install command with  "-r" to reverts the apply by
    // telling the bootloader to boot into the current partition

    // This is equivalent to : command << c_installScript << " -l " << logFolder << " -r"

    std::string command = adushconst::adu_shell;
    std::vector<std::string> args{ adushconst::update_type_opt,       adushconst::update_type_microsoft_swupdate,
                                   adushconst::update_action_opt,     adushconst::update_action_apply,
                                   adushconst::target_log_folder_opt, logFolder };

    std::string output;

    const int exitCode = ADUC_LaunchChildProcess(command, args, output);
    if (exitCode != 0)
    {
        // If failed to cancel apply, apply should return SuccessRebootRequired.
        Log_Error("Failed to cancel Apply, extendedResultCode = %d", exitCode);
        return ADUC_Result{ ADUC_CancelResult_Failure, exitCode };
    }

    Log_Info("Apply was cancelled");
    return ADUC_Result{ ADUC_ApplyResult_Cancelled };
}
