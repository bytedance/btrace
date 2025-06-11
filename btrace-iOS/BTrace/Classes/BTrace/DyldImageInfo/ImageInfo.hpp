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

#ifndef BTRACE_IMAGE_INFO_H_
#define BTRACE_IMAGE_INFO_H_

#include <stdint.h>
#include <stdbool.h>
#include <mach/mach.h>
#include <mach-o/loader.h>

#ifdef __cplusplus

#include <mutex>
#include <unordered_map>

#include "globals.hpp"

#ifdef ARCH_IS_64_BIT
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
#define LC_SEGMENT_ARCH LC_SEGMENT_64
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
#define LC_SEGMENT_ARCH LC_SEGMENT
#endif

namespace btrace {
    class ImageInfo;

    typedef void (*ImageInfoCallback)(btrace::ImageInfo* image_info);

    class ImageInfo
    {
    public:
        ImageInfo(vm_address_t header, intptr_t vmaddr_slide);

        ~ImageInfo()
        {
            if (name != NULL)
            {
                free(name);
            }
        }

        static void Init();
        static void Start();
        static void Stop();
        static void ConstructImages();

        static ImageInfo *CreateImageInfo(const struct mach_header *header, intptr_t vmaddr_slide);
        

        static void DyldAddImageCallback(const struct mach_header *mh, intptr_t vmaddr_slide);
        static void DyldRemoveImageCallback(const struct mach_header *mh, intptr_t vmaddr_slide);
        
        enum Type : uint32_t
        {
            kAdd,
            kRemove
        };

        Type type;
        vm_address_t header_addr;
        char *name;
        vm_address_t text_vmaddr;
        vm_size_t text_size;
        char uuid[40];
        bool dumped = false;

    private:
        static void AddToMap(ImageInfo *image_info);
        static void RemoveFromMap(vm_address_t header);

        static void RegisterCallback();
        
        static inline bool started_ = false;
        static inline std::mutex *image_info_lock_;
        static inline std::once_flag dyld_callback_flag_;
        static inline std::unordered_map<uint64_t, ImageInfo *>* image_map_ = nullptr;

        friend class ImageInfoIterator;
    };

    class ImageInfoIterator
    {
    public:
        ImageInfoIterator();
        ~ImageInfoIterator();

        // Returns false when there are no more threads left.
        bool HasNext() const;

        // Returns the current thread and moves forward.
        ImageInfo *Next();

    private:
        std::unordered_map<uint64_t, ImageInfo *>::iterator it_;
    };

}

#endif

#endif /* BTRACE_IMAGE_INFO_H_ */
