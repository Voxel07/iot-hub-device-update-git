/**
 * @file swupdate_handler.hpp
 * @brief Defines SWUpdateHandlerImpl.
 *
 * @copyright Copyright (c) 2019, Microsoft Corp.
 */
#ifndef ADUC_SWUPDATE_HANDLER_HPP
#define ADUC_SWUPDATE_HANDLER_HPP

#include "aduc/content_handler.hpp"
#include "aduc/content_handler_factory.hpp"
#include <aduc/result.h>
#include <memory>
#include <string>

/**
 * @brief handler creation function
 * This function calls  CreateContentHandler from handler factory 
 */
std::unique_ptr<ContentHandler> fus_fsupdate_CreateFunc(const ContentHandlerCreateData& data);

/**
 * @class SWUpdateHandlerImpl
 * @brief The swupdate specific implementation of ContentHandler interface.
 */
class FSUpdateHandlerImpl : public ContentHandler
{
public:
    static std::unique_ptr<ContentHandler>
    CreateContentHandler(const std::string& workFolder, const std::string& logFolder, const std::string& filename, const std::string& fileType);

    // Delete copy ctor, copy assignment, move ctor and move assignment operators.
    FSUpdateHandlerImpl(const FSUpdateHandlerImpl&) = delete;
    FSUpdateHandlerImpl& operator=(const FSUpdateHandlerImpl&) = delete;
    FSUpdateHandlerImpl(FSUpdateHandlerImpl&&) = delete;
    FSUpdateHandlerImpl& operator=(FSUpdateHandlerImpl&&) = delete;

    ~FSUpdateHandlerImpl() override = default;

    ADUC_Result Prepare(const ADUC_PrepareInfo* prepareInfo) override;
    ADUC_Result Download() override;
    ADUC_Result Install(/*const std::string& updateType*/) override;
    ADUC_Result Apply() override;
    ADUC_Result Cancel() override;
    ADUC_Result IsInstalled(const std::string& installedCriteria) override;
    ADUC_Result UpdateVersionFile(const std::string& newVersion) override;
    static std::string ReadValueFromFile(const std::string& filePath);

protected:
    // Protected constructor, must call CreateContentHandler factory method or from derived simulator class
    FSUpdateHandlerImpl(const std::string& workFolder, const std::string& logFolder, const std::string& filename, const std::string& fileType) :
        _workFolder{ workFolder }, _logFolder{ logFolder }, _filename{ filename }, _fileType{ fileType }
    {
    }

private:
    std::string _workFolder;
    std::string _logFolder;
    std::string _filename;
    std::string _fileType;
    bool _isApply{ false };
};

#endif // ADUC_SWUPDATE_HANDLER_HPP
