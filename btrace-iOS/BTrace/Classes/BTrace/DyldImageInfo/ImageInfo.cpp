/*
 * Copyright (C) 2025 ByteDance Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

#include "ImageInfo.hpp"


namespace btrace
{

    extern void image_info_callback(ImageInfo *image_info);


    void ImageInfo::Init()
    {
        if (image_info_lock_ == nullptr) {
            // do not delete image_info_lock_
            image_info_lock_ = new std::mutex();
        }
    }

    void ImageInfo::RegisterCallback() {
        _dyld_register_func_for_add_image(DyldAddImageCallback);
//        _dyld_register_func_for_remove_image(DyldRemoveImageCallback);
    }

    void ImageInfo::Start() {
        {
            std::lock_guard<std::mutex> ml(*image_info_lock_);
            if (image_map_ == nullptr) {
                image_map_ = new std::unordered_map<uint64_t, ImageInfo *>();
            }
            started_ = true;
        }
        
        std::call_once(dyld_callback_flag_, RegisterCallback);
        ConstructImages();
    }

    void ImageInfo::Stop()
    {
        {
            std::lock_guard<std::mutex> ml(*image_info_lock_);

            if (!started_)
            {
                return;
            }

            for (auto it = image_map_->begin(); it != image_map_->end(); ++it)
            {
                ImageInfo *image_info = it->second;
                delete image_info;
            }

            delete image_map_;
            image_map_ = nullptr;
            started_ = false;
        }
    }

    ImageInfo::ImageInfo(vm_address_t header, intptr_t vmaddr_slide)
        : text_vmaddr(0),
          text_size(0),
          header_addr(header),
          name(NULL),
          type(kAdd)
    {
        Dl_info info;
        if (dladdr((mach_header_t *)header_addr, &info))
        {
            name = strdup(info.dli_fname);
        }

        uintptr_t cmdptr = header + sizeof(mach_header_t);

        for (int index = 0; index < ((mach_header_t *)header_addr)->ncmds; ++index)
        {
            struct load_command *loadcmd = (struct load_command *)cmdptr;
            if (loadcmd->cmd == LC_SEGMENT_ARCH)
            {
                segment_command_t *segment = (segment_command_t *)cmdptr;
                if (strncmp(segment->segname, SEG_TEXT, sizeof(segment->segname)) == 0)
                {
                    text_size = segment->vmsize;
                    text_vmaddr = segment->vmaddr + vmaddr_slide;
                }
            }
            else if (loadcmd->cmd == LC_UUID)
            {
                struct uuid_command *uuid_cmd = (struct uuid_command *)cmdptr;
                static char hex_table[] = "0123456789abcdef";
                for (size_t i = 0; i < 16; i++)
                {
                    unsigned char current = (unsigned char)uuid_cmd->uuid[i];
                    uuid[i * 2] = hex_table[(current >> 4) & 0xF];
                    uuid[i * 2 + 1] = hex_table[current & 0xF];
                }
                uuid[32] = 0;
            }
            cmdptr += loadcmd->cmdsize;
        }
    }

    ImageInfo *ImageInfo::CreateImageInfo(const struct mach_header *header, intptr_t vmaddr_slide)
    {
        if (!started_) {
            return nullptr;
        }
        
        std::lock_guard<std::mutex> ml(*image_info_lock_);
        
        if (!started_) {
            return nullptr;
        }
        
        ImageInfo *image_info = nullptr;
        
        auto id = (uint64_t)header;
        auto it = image_map_->find(id);
        if (it != image_map_->end()) {
            image_info = it->second;
        } else {
            image_info = new ImageInfo((vm_address_t)header, vmaddr_slide);
            AddToMap(image_info);
        }

        return image_info;
    }

    void ImageInfo::ConstructImages()
    {
        for (uint32_t i = 0; i < _dyld_image_count(); i++) {
            const struct mach_header* header = _dyld_get_image_header(i);
            intptr_t slide = _dyld_get_image_vmaddr_slide(i);
            CreateImageInfo(header, slide);
        }
    }

    void ImageInfo::DyldAddImageCallback(const struct mach_header *mh, intptr_t vmaddr_slide)
    {
        auto image_info = CreateImageInfo(mh, vmaddr_slide);
        image_info_callback(image_info);
    }

    void ImageInfo::DyldRemoveImageCallback(const struct mach_header *mh, intptr_t vmaddr_slide)
    {
        RemoveFromMap((vm_address_t)mh);
    }

    void ImageInfo::AddToMap(ImageInfo *image_info)
    {
        image_map_->emplace(image_info->header_addr, image_info);
    }

    void ImageInfo::RemoveFromMap(vm_address_t header)
    {
        if (!started_) {
            return;
        }
        
        std::lock_guard<std::mutex> ml(*image_info_lock_);
        
        if (!started_) {
            return;
        }
        
        image_map_->erase(header);
    }

    ImageInfoIterator::ImageInfoIterator()
    {
        // Lock the image info list while iterating.
        ImageInfo::image_info_lock_->lock();
        it_ = ImageInfo::image_map_->begin();
    }

    ImageInfoIterator::~ImageInfoIterator()
    {
        // Unlock the image info list when done.
        ImageInfo::image_info_lock_->unlock();
    }

    bool ImageInfoIterator::HasNext() const
    {
        return it_ != ImageInfo::image_map_->end();
    }

    ImageInfo *ImageInfoIterator::Next()
    {
        ImageInfo *current = it_->second;
        ++it_;
        return current;
    }
} // namespace btrace
