#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <stdio.h>


size_t readFile(char *path, char **buff)
{
    FILE *f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    *buff = malloc(fsize + 1);
    fread(*buff, 1, fsize, f);
    fclose(f);

    return fsize;
}

int main()
{
    VkInstance instance;

    // make the app info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VNN";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VNN";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    uint32_t physicalDeviceCount = 0;

    // make the app create info struct
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // create the instance
    VkResult result = vkCreateInstance(&createInfo, 0, &instance);

    // get the physical devices
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0);
    VkPhysicalDevice* physicalDevices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);

    // print out the device info
    printf("available devices: %i\n", physicalDeviceCount);

    // find the queue we're going to use
    VkPhysicalDevice physicalDevice = NULL;
    VkDevice device = NULL;
    VkQueue queue = NULL;
    uint32_t queueIdx = -1;
    for (size_t i = 0; i < physicalDeviceCount; ++i)
    {
        // get the queues for the device
        uint32_t queueFamilyPropertiesCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertiesCount, 0);
        VkQueueFamilyProperties* queueFamilyProperties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyPropertiesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyPropertiesCount, queueFamilyProperties);

        printf("available queues on device %lu: %i\n", i, queueFamilyPropertiesCount);
        // find a queue that has compute and transfer
        for (size_t j = 0; j < queueFamilyPropertiesCount; ++j)
        {
            if (queueFamilyProperties[j].queueFlags & (VK_QUEUE_COMPUTE_BIT && VK_QUEUE_TRANSFER_BIT))
            {
                printf("queue %lu on device %lu is suitable\n", j, i);
                queueIdx = j;
                break;
            }
        }

        // clean up
        free(queueFamilyProperties);

        if (queueIdx != -1)
        {
            float queuePriority = 1.0f;

            physicalDevice = physicalDevices[i];

            VkDeviceQueueCreateInfo queueCreateInfo;
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.pNext = NULL;
            queueCreateInfo.flags = 0;
            queueCreateInfo.queueFamilyIndex = queueIdx;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            VkDeviceCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pNext = NULL;
            createInfo.flags = 0;
            createInfo.queueCreateInfoCount = 1;
            createInfo.pQueueCreateInfos = &queueCreateInfo;
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = NULL;
            createInfo.enabledExtensionCount = 0;
            createInfo.ppEnabledExtensionNames = NULL;
            createInfo.pEnabledFeatures = NULL;

            vkCreateDevice(physicalDevice, &createInfo, 0, &device);

            break;
        }
    }

    // get device memory type
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    const int32_t bufferLength = 16384;
    const uint32_t bufferSize = sizeof(int32_t) * bufferLength;
    const VkDeviceSize memorySize = bufferSize * 2; 
    uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;
    for (uint32_t k = 0; k < properties.memoryTypeCount; k++) {
        if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & properties.memoryTypes[k].propertyFlags) &&
            (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & properties.memoryTypes[k].propertyFlags) &&
            (memorySize < properties.memoryHeaps[properties.memoryTypes[k].heapIndex].size))
        {
            memoryTypeIndex = k;
            break;
        }
    }

    // allocate some memory
    VkMemoryAllocateInfo memoryAllocInfo;
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.pNext = NULL;
    memoryAllocInfo.allocationSize = memorySize;
    memoryAllocInfo.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory memory;
    vkAllocateMemory(device, &memoryAllocInfo, 0, &memory);

    // map the memory and fill it
    uint32_t *payload;
    vkMapMemory(device, memory, 0, memorySize, 0, (void *)&payload);
    for (uint32_t i = 0; i < memorySize / sizeof(int32_t); i++) {
      payload[i] = rand();
    }
    vkUnmapMemory(device, memory);

    // turn the memory into 2 buffers
    VkBufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    bufferCreateInfo.pNext = 0;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &queueIdx;

    VkBuffer inBuffer;
    vkCreateBuffer(device, &bufferCreateInfo, 0, &inBuffer);
    vkBindBufferMemory(device, inBuffer, memory, 0);

    VkBuffer outBuffer;
    vkCreateBuffer(device, &bufferCreateInfo, 0, &outBuffer);
    vkBindBufferMemory(device, outBuffer, memory, bufferSize);

    // load the shader
    char *shader;
    size_t shaderSize = readFile("shader.spv", &shader);

    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = 0;
    shaderModuleCreateInfo.flags = 0;
    shaderModuleCreateInfo.codeSize = shaderSize;
    shaderModuleCreateInfo.pCode = (uint32_t*)shader;

    VkShaderModule shaderModule;
    vkCreateShaderModule(device, &shaderModuleCreateInfo, 0, &shaderModule);

    printf("created shader of size %lu\n", shaderSize);

    // create the descriptor set
    VkDescriptorSetLayoutBinding layoutBindings[2];
    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[0].pImmutableSamplers = 0;
    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layoutBindings[1].pImmutableSamplers = 0;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext = 0;
    layoutCreateInfo.flags = 0;
    layoutCreateInfo.bindingCount = 2;
    layoutCreateInfo.pBindings = layoutBindings;

    VkDescriptorSetLayout descriptorSetLayout;
    vkCreateDescriptorSetLayout(device, &layoutCreateInfo, 0, &descriptorSetLayout);

    // make the pipeline
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = 0;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = 0;

    // TODO finish the code here

    // clean up
    free(physicalDevices);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
}
