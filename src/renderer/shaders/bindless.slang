// How every shader reaches its textures and samplers: a DescriptorHandle<T> indexes the array of
// T's descriptor type in the bindless set, which the renderer declares to match. Slang's default
// puts every type on one binding through VK_EXT_mutable_descriptor_type, which many devices lack,
// so this asks for one binding per type instead. A shader that uses descriptor handles imports this
// module. Slang looks the function up by this name, which is why it isn't snake_case.
#language slang 2026
module bindless;

public export T getDescriptorFromHandle<T>(DescriptorHandle<T> handle)
    where T : IOpaqueDescriptor {
    return defaultGetDescriptorFromHandle(handle, BindlessDescriptorOptions.None);
}
