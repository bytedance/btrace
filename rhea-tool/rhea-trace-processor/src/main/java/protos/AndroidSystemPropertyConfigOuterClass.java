// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: protos/perfetto/config/android/android_system_property_config.proto

package perfetto.protos;

public final class AndroidSystemPropertyConfigOuterClass {
  private AndroidSystemPropertyConfigOuterClass() {}
  public static void registerAllExtensions(
      com.google.protobuf.ExtensionRegistryLite registry) {
  }

  public static void registerAllExtensions(
      com.google.protobuf.ExtensionRegistry registry) {
    registerAllExtensions(
        (com.google.protobuf.ExtensionRegistryLite) registry);
  }
  public interface AndroidSystemPropertyConfigOrBuilder extends
      // @@protoc_insertion_point(interface_extends:perfetto.protos.AndroidSystemPropertyConfig)
      com.google.protobuf.MessageOrBuilder {

    /**
     * <pre>
     * Frequency of polling. If absent the state will be recorded once, at the
     * start of the trace.
     * This is required to be &gt; 100ms to avoid excessive CPU usage.
     * </pre>
     *
     * <code>optional uint32 poll_ms = 1;</code>
     * @return Whether the pollMs field is set.
     */
    boolean hasPollMs();
    /**
     * <pre>
     * Frequency of polling. If absent the state will be recorded once, at the
     * start of the trace.
     * This is required to be &gt; 100ms to avoid excessive CPU usage.
     * </pre>
     *
     * <code>optional uint32 poll_ms = 1;</code>
     * @return The pollMs.
     */
    int getPollMs();

    /**
     * <pre>
     * Properties to poll. All property names must start with "debug.tracing.".
     * </pre>
     *
     * <code>repeated string property_name = 2;</code>
     * @return A list containing the propertyName.
     */
    java.util.List<java.lang.String>
        getPropertyNameList();
    /**
     * <pre>
     * Properties to poll. All property names must start with "debug.tracing.".
     * </pre>
     *
     * <code>repeated string property_name = 2;</code>
     * @return The count of propertyName.
     */
    int getPropertyNameCount();
    /**
     * <pre>
     * Properties to poll. All property names must start with "debug.tracing.".
     * </pre>
     *
     * <code>repeated string property_name = 2;</code>
     * @param index The index of the element to return.
     * @return The propertyName at the given index.
     */
    java.lang.String getPropertyName(int index);
    /**
     * <pre>
     * Properties to poll. All property names must start with "debug.tracing.".
     * </pre>
     *
     * <code>repeated string property_name = 2;</code>
     * @param index The index of the value to return.
     * @return The bytes of the propertyName at the given index.
     */
    com.google.protobuf.ByteString
        getPropertyNameBytes(int index);
  }
  /**
   * <pre>
   * Data source that polls for system properties.
   * </pre>
   *
   * Protobuf type {@code perfetto.protos.AndroidSystemPropertyConfig}
   */
  public static final class AndroidSystemPropertyConfig extends
      com.google.protobuf.GeneratedMessageV3 implements
      // @@protoc_insertion_point(message_implements:perfetto.protos.AndroidSystemPropertyConfig)
      AndroidSystemPropertyConfigOrBuilder {
  private static final long serialVersionUID = 0L;
    // Use AndroidSystemPropertyConfig.newBuilder() to construct.
    private AndroidSystemPropertyConfig(com.google.protobuf.GeneratedMessageV3.Builder<?> builder) {
      super(builder);
    }
    private AndroidSystemPropertyConfig() {
      propertyName_ = com.google.protobuf.LazyStringArrayList.EMPTY;
    }

    @java.lang.Override
    @SuppressWarnings({"unused"})
    protected java.lang.Object newInstance(
        UnusedPrivateParameter unused) {
      return new AndroidSystemPropertyConfig();
    }

    @java.lang.Override
    public final com.google.protobuf.UnknownFieldSet
    getUnknownFields() {
      return this.unknownFields;
    }
    public static final com.google.protobuf.Descriptors.Descriptor
        getDescriptor() {
      return perfetto.protos.AndroidSystemPropertyConfigOuterClass.internal_static_perfetto_protos_AndroidSystemPropertyConfig_descriptor;
    }

    @java.lang.Override
    protected com.google.protobuf.GeneratedMessageV3.FieldAccessorTable
        internalGetFieldAccessorTable() {
      return perfetto.protos.AndroidSystemPropertyConfigOuterClass.internal_static_perfetto_protos_AndroidSystemPropertyConfig_fieldAccessorTable
          .ensureFieldAccessorsInitialized(
              perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig.class, perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig.Builder.class);
    }

    private int bitField0_;
    public static final int POLL_MS_FIELD_NUMBER = 1;
    private int pollMs_ = 0;
    /**
     * <pre>
     * Frequency of polling. If absent the state will be recorded once, at the
     * start of the trace.
     * This is required to be &gt; 100ms to avoid excessive CPU usage.
     * </pre>
     *
     * <code>optional uint32 poll_ms = 1;</code>
     * @return Whether the pollMs field is set.
     */
    @java.lang.Override
    public boolean hasPollMs() {
      return ((bitField0_ & 0x00000001) != 0);
    }
    /**
     * <pre>
     * Frequency of polling. If absent the state will be recorded once, at the
     * start of the trace.
     * This is required to be &gt; 100ms to avoid excessive CPU usage.
     * </pre>
     *
     * <code>optional uint32 poll_ms = 1;</code>
     * @return The pollMs.
     */
    @java.lang.Override
    public int getPollMs() {
      return pollMs_;
    }

    public static final int PROPERTY_NAME_FIELD_NUMBER = 2;
    @SuppressWarnings("serial")
    private com.google.protobuf.LazyStringList propertyName_;
    /**
     * <pre>
     * Properties to poll. All property names must start with "debug.tracing.".
     * </pre>
     *
     * <code>repeated string property_name = 2;</code>
     * @return A list containing the propertyName.
     */
    public com.google.protobuf.ProtocolStringList
        getPropertyNameList() {
      return propertyName_;
    }
    /**
     * <pre>
     * Properties to poll. All property names must start with "debug.tracing.".
     * </pre>
     *
     * <code>repeated string property_name = 2;</code>
     * @return The count of propertyName.
     */
    public int getPropertyNameCount() {
      return propertyName_.size();
    }
    /**
     * <pre>
     * Properties to poll. All property names must start with "debug.tracing.".
     * </pre>
     *
     * <code>repeated string property_name = 2;</code>
     * @param index The index of the element to return.
     * @return The propertyName at the given index.
     */
    public java.lang.String getPropertyName(int index) {
      return propertyName_.get(index);
    }
    /**
     * <pre>
     * Properties to poll. All property names must start with "debug.tracing.".
     * </pre>
     *
     * <code>repeated string property_name = 2;</code>
     * @param index The index of the value to return.
     * @return The bytes of the propertyName at the given index.
     */
    public com.google.protobuf.ByteString
        getPropertyNameBytes(int index) {
      return propertyName_.getByteString(index);
    }

    private byte memoizedIsInitialized = -1;
    @java.lang.Override
    public final boolean isInitialized() {
      byte isInitialized = memoizedIsInitialized;
      if (isInitialized == 1) return true;
      if (isInitialized == 0) return false;

      memoizedIsInitialized = 1;
      return true;
    }

    @java.lang.Override
    public void writeTo(com.google.protobuf.CodedOutputStream output)
                        throws java.io.IOException {
      if (((bitField0_ & 0x00000001) != 0)) {
        output.writeUInt32(1, pollMs_);
      }
      for (int i = 0; i < propertyName_.size(); i++) {
        com.google.protobuf.GeneratedMessageV3.writeString(output, 2, propertyName_.getRaw(i));
      }
      getUnknownFields().writeTo(output);
    }

    @java.lang.Override
    public int getSerializedSize() {
      int size = memoizedSize;
      if (size != -1) return size;

      size = 0;
      if (((bitField0_ & 0x00000001) != 0)) {
        size += com.google.protobuf.CodedOutputStream
          .computeUInt32Size(1, pollMs_);
      }
      {
        int dataSize = 0;
        for (int i = 0; i < propertyName_.size(); i++) {
          dataSize += computeStringSizeNoTag(propertyName_.getRaw(i));
        }
        size += dataSize;
        size += 1 * getPropertyNameList().size();
      }
      size += getUnknownFields().getSerializedSize();
      memoizedSize = size;
      return size;
    }

    @java.lang.Override
    public boolean equals(final java.lang.Object obj) {
      if (obj == this) {
       return true;
      }
      if (!(obj instanceof perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig)) {
        return super.equals(obj);
      }
      perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig other = (perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig) obj;

      if (hasPollMs() != other.hasPollMs()) return false;
      if (hasPollMs()) {
        if (getPollMs()
            != other.getPollMs()) return false;
      }
      if (!getPropertyNameList()
          .equals(other.getPropertyNameList())) return false;
      if (!getUnknownFields().equals(other.getUnknownFields())) return false;
      return true;
    }

    @java.lang.Override
    public int hashCode() {
      if (memoizedHashCode != 0) {
        return memoizedHashCode;
      }
      int hash = 41;
      hash = (19 * hash) + getDescriptor().hashCode();
      if (hasPollMs()) {
        hash = (37 * hash) + POLL_MS_FIELD_NUMBER;
        hash = (53 * hash) + getPollMs();
      }
      if (getPropertyNameCount() > 0) {
        hash = (37 * hash) + PROPERTY_NAME_FIELD_NUMBER;
        hash = (53 * hash) + getPropertyNameList().hashCode();
      }
      hash = (29 * hash) + getUnknownFields().hashCode();
      memoizedHashCode = hash;
      return hash;
    }

    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(
        java.nio.ByteBuffer data)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(
        java.nio.ByteBuffer data,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data, extensionRegistry);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(
        com.google.protobuf.ByteString data)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(
        com.google.protobuf.ByteString data,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data, extensionRegistry);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(byte[] data)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(
        byte[] data,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data, extensionRegistry);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(java.io.InputStream input)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessageV3
          .parseWithIOException(PARSER, input);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(
        java.io.InputStream input,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessageV3
          .parseWithIOException(PARSER, input, extensionRegistry);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseDelimitedFrom(java.io.InputStream input)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessageV3
          .parseDelimitedWithIOException(PARSER, input);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseDelimitedFrom(
        java.io.InputStream input,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessageV3
          .parseDelimitedWithIOException(PARSER, input, extensionRegistry);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(
        com.google.protobuf.CodedInputStream input)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessageV3
          .parseWithIOException(PARSER, input);
    }
    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig parseFrom(
        com.google.protobuf.CodedInputStream input,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessageV3
          .parseWithIOException(PARSER, input, extensionRegistry);
    }

    @java.lang.Override
    public Builder newBuilderForType() { return newBuilder(); }
    public static Builder newBuilder() {
      return DEFAULT_INSTANCE.toBuilder();
    }
    public static Builder newBuilder(perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig prototype) {
      return DEFAULT_INSTANCE.toBuilder().mergeFrom(prototype);
    }
    @java.lang.Override
    public Builder toBuilder() {
      return this == DEFAULT_INSTANCE
          ? new Builder() : new Builder().mergeFrom(this);
    }

    @java.lang.Override
    protected Builder newBuilderForType(
        com.google.protobuf.GeneratedMessageV3.BuilderParent parent) {
      Builder builder = new Builder(parent);
      return builder;
    }
    /**
     * <pre>
     * Data source that polls for system properties.
     * </pre>
     *
     * Protobuf type {@code perfetto.protos.AndroidSystemPropertyConfig}
     */
    public static final class Builder extends
        com.google.protobuf.GeneratedMessageV3.Builder<Builder> implements
        // @@protoc_insertion_point(builder_implements:perfetto.protos.AndroidSystemPropertyConfig)
        perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfigOrBuilder {
      public static final com.google.protobuf.Descriptors.Descriptor
          getDescriptor() {
        return perfetto.protos.AndroidSystemPropertyConfigOuterClass.internal_static_perfetto_protos_AndroidSystemPropertyConfig_descriptor;
      }

      @java.lang.Override
      protected com.google.protobuf.GeneratedMessageV3.FieldAccessorTable
          internalGetFieldAccessorTable() {
        return perfetto.protos.AndroidSystemPropertyConfigOuterClass.internal_static_perfetto_protos_AndroidSystemPropertyConfig_fieldAccessorTable
            .ensureFieldAccessorsInitialized(
                perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig.class, perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig.Builder.class);
      }

      // Construct using perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig.newBuilder()
      private Builder() {

      }

      private Builder(
          com.google.protobuf.GeneratedMessageV3.BuilderParent parent) {
        super(parent);

      }
      @java.lang.Override
      public Builder clear() {
        super.clear();
        bitField0_ = 0;
        pollMs_ = 0;
        propertyName_ = com.google.protobuf.LazyStringArrayList.EMPTY;
        bitField0_ = (bitField0_ & ~0x00000002);
        return this;
      }

      @java.lang.Override
      public com.google.protobuf.Descriptors.Descriptor
          getDescriptorForType() {
        return perfetto.protos.AndroidSystemPropertyConfigOuterClass.internal_static_perfetto_protos_AndroidSystemPropertyConfig_descriptor;
      }

      @java.lang.Override
      public perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig getDefaultInstanceForType() {
        return perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig.getDefaultInstance();
      }

      @java.lang.Override
      public perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig build() {
        perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig result = buildPartial();
        if (!result.isInitialized()) {
          throw newUninitializedMessageException(result);
        }
        return result;
      }

      @java.lang.Override
      public perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig buildPartial() {
        perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig result = new perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig(this);
        buildPartialRepeatedFields(result);
        if (bitField0_ != 0) { buildPartial0(result); }
        onBuilt();
        return result;
      }

      private void buildPartialRepeatedFields(perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig result) {
        if (((bitField0_ & 0x00000002) != 0)) {
          propertyName_ = propertyName_.getUnmodifiableView();
          bitField0_ = (bitField0_ & ~0x00000002);
        }
        result.propertyName_ = propertyName_;
      }

      private void buildPartial0(perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig result) {
        int from_bitField0_ = bitField0_;
        int to_bitField0_ = 0;
        if (((from_bitField0_ & 0x00000001) != 0)) {
          result.pollMs_ = pollMs_;
          to_bitField0_ |= 0x00000001;
        }
        result.bitField0_ |= to_bitField0_;
      }

      @java.lang.Override
      public Builder clone() {
        return super.clone();
      }
      @java.lang.Override
      public Builder setField(
          com.google.protobuf.Descriptors.FieldDescriptor field,
          java.lang.Object value) {
        return super.setField(field, value);
      }
      @java.lang.Override
      public Builder clearField(
          com.google.protobuf.Descriptors.FieldDescriptor field) {
        return super.clearField(field);
      }
      @java.lang.Override
      public Builder clearOneof(
          com.google.protobuf.Descriptors.OneofDescriptor oneof) {
        return super.clearOneof(oneof);
      }
      @java.lang.Override
      public Builder setRepeatedField(
          com.google.protobuf.Descriptors.FieldDescriptor field,
          int index, java.lang.Object value) {
        return super.setRepeatedField(field, index, value);
      }
      @java.lang.Override
      public Builder addRepeatedField(
          com.google.protobuf.Descriptors.FieldDescriptor field,
          java.lang.Object value) {
        return super.addRepeatedField(field, value);
      }
      @java.lang.Override
      public Builder mergeFrom(com.google.protobuf.Message other) {
        if (other instanceof perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig) {
          return mergeFrom((perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig)other);
        } else {
          super.mergeFrom(other);
          return this;
        }
      }

      public Builder mergeFrom(perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig other) {
        if (other == perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig.getDefaultInstance()) return this;
        if (other.hasPollMs()) {
          setPollMs(other.getPollMs());
        }
        if (!other.propertyName_.isEmpty()) {
          if (propertyName_.isEmpty()) {
            propertyName_ = other.propertyName_;
            bitField0_ = (bitField0_ & ~0x00000002);
          } else {
            ensurePropertyNameIsMutable();
            propertyName_.addAll(other.propertyName_);
          }
          onChanged();
        }
        this.mergeUnknownFields(other.getUnknownFields());
        onChanged();
        return this;
      }

      @java.lang.Override
      public final boolean isInitialized() {
        return true;
      }

      @java.lang.Override
      public Builder mergeFrom(
          com.google.protobuf.CodedInputStream input,
          com.google.protobuf.ExtensionRegistryLite extensionRegistry)
          throws java.io.IOException {
        if (extensionRegistry == null) {
          throw new java.lang.NullPointerException();
        }
        try {
          boolean done = false;
          while (!done) {
            int tag = input.readTag();
            switch (tag) {
              case 0:
                done = true;
                break;
              case 8: {
                pollMs_ = input.readUInt32();
                bitField0_ |= 0x00000001;
                break;
              } // case 8
              case 18: {
                com.google.protobuf.ByteString bs = input.readBytes();
                ensurePropertyNameIsMutable();
                propertyName_.add(bs);
                break;
              } // case 18
              default: {
                if (!super.parseUnknownField(input, extensionRegistry, tag)) {
                  done = true; // was an endgroup tag
                }
                break;
              } // default:
            } // switch (tag)
          } // while (!done)
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
          throw e.unwrapIOException();
        } finally {
          onChanged();
        } // finally
        return this;
      }
      private int bitField0_;

      private int pollMs_ ;
      /**
       * <pre>
       * Frequency of polling. If absent the state will be recorded once, at the
       * start of the trace.
       * This is required to be &gt; 100ms to avoid excessive CPU usage.
       * </pre>
       *
       * <code>optional uint32 poll_ms = 1;</code>
       * @return Whether the pollMs field is set.
       */
      @java.lang.Override
      public boolean hasPollMs() {
        return ((bitField0_ & 0x00000001) != 0);
      }
      /**
       * <pre>
       * Frequency of polling. If absent the state will be recorded once, at the
       * start of the trace.
       * This is required to be &gt; 100ms to avoid excessive CPU usage.
       * </pre>
       *
       * <code>optional uint32 poll_ms = 1;</code>
       * @return The pollMs.
       */
      @java.lang.Override
      public int getPollMs() {
        return pollMs_;
      }
      /**
       * <pre>
       * Frequency of polling. If absent the state will be recorded once, at the
       * start of the trace.
       * This is required to be &gt; 100ms to avoid excessive CPU usage.
       * </pre>
       *
       * <code>optional uint32 poll_ms = 1;</code>
       * @param value The pollMs to set.
       * @return This builder for chaining.
       */
      public Builder setPollMs(int value) {
        
        pollMs_ = value;
        bitField0_ |= 0x00000001;
        onChanged();
        return this;
      }
      /**
       * <pre>
       * Frequency of polling. If absent the state will be recorded once, at the
       * start of the trace.
       * This is required to be &gt; 100ms to avoid excessive CPU usage.
       * </pre>
       *
       * <code>optional uint32 poll_ms = 1;</code>
       * @return This builder for chaining.
       */
      public Builder clearPollMs() {
        bitField0_ = (bitField0_ & ~0x00000001);
        pollMs_ = 0;
        onChanged();
        return this;
      }

      private com.google.protobuf.LazyStringList propertyName_ = com.google.protobuf.LazyStringArrayList.EMPTY;
      private void ensurePropertyNameIsMutable() {
        if (!((bitField0_ & 0x00000002) != 0)) {
          propertyName_ = new com.google.protobuf.LazyStringArrayList(propertyName_);
          bitField0_ |= 0x00000002;
         }
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @return A list containing the propertyName.
       */
      public com.google.protobuf.ProtocolStringList
          getPropertyNameList() {
        return propertyName_.getUnmodifiableView();
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @return The count of propertyName.
       */
      public int getPropertyNameCount() {
        return propertyName_.size();
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @param index The index of the element to return.
       * @return The propertyName at the given index.
       */
      public java.lang.String getPropertyName(int index) {
        return propertyName_.get(index);
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @param index The index of the value to return.
       * @return The bytes of the propertyName at the given index.
       */
      public com.google.protobuf.ByteString
          getPropertyNameBytes(int index) {
        return propertyName_.getByteString(index);
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @param index The index to set the value at.
       * @param value The propertyName to set.
       * @return This builder for chaining.
       */
      public Builder setPropertyName(
          int index, java.lang.String value) {
        if (value == null) { throw new NullPointerException(); }
        ensurePropertyNameIsMutable();
        propertyName_.set(index, value);
        onChanged();
        return this;
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @param value The propertyName to add.
       * @return This builder for chaining.
       */
      public Builder addPropertyName(
          java.lang.String value) {
        if (value == null) { throw new NullPointerException(); }
        ensurePropertyNameIsMutable();
        propertyName_.add(value);
        onChanged();
        return this;
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @param values The propertyName to add.
       * @return This builder for chaining.
       */
      public Builder addAllPropertyName(
          java.lang.Iterable<java.lang.String> values) {
        ensurePropertyNameIsMutable();
        com.google.protobuf.AbstractMessageLite.Builder.addAll(
            values, propertyName_);
        onChanged();
        return this;
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @return This builder for chaining.
       */
      public Builder clearPropertyName() {
        propertyName_ = com.google.protobuf.LazyStringArrayList.EMPTY;
        bitField0_ = (bitField0_ & ~0x00000002);
        onChanged();
        return this;
      }
      /**
       * <pre>
       * Properties to poll. All property names must start with "debug.tracing.".
       * </pre>
       *
       * <code>repeated string property_name = 2;</code>
       * @param value The bytes of the propertyName to add.
       * @return This builder for chaining.
       */
      public Builder addPropertyNameBytes(
          com.google.protobuf.ByteString value) {
        if (value == null) { throw new NullPointerException(); }
        ensurePropertyNameIsMutable();
        propertyName_.add(value);
        onChanged();
        return this;
      }
      @java.lang.Override
      public final Builder setUnknownFields(
          final com.google.protobuf.UnknownFieldSet unknownFields) {
        return super.setUnknownFields(unknownFields);
      }

      @java.lang.Override
      public final Builder mergeUnknownFields(
          final com.google.protobuf.UnknownFieldSet unknownFields) {
        return super.mergeUnknownFields(unknownFields);
      }


      // @@protoc_insertion_point(builder_scope:perfetto.protos.AndroidSystemPropertyConfig)
    }

    // @@protoc_insertion_point(class_scope:perfetto.protos.AndroidSystemPropertyConfig)
    private static final perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig DEFAULT_INSTANCE;
    static {
      DEFAULT_INSTANCE = new perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig();
    }

    public static perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig getDefaultInstance() {
      return DEFAULT_INSTANCE;
    }

    @java.lang.Deprecated public static final com.google.protobuf.Parser<AndroidSystemPropertyConfig>
        PARSER = new com.google.protobuf.AbstractParser<AndroidSystemPropertyConfig>() {
      @java.lang.Override
      public AndroidSystemPropertyConfig parsePartialFrom(
          com.google.protobuf.CodedInputStream input,
          com.google.protobuf.ExtensionRegistryLite extensionRegistry)
          throws com.google.protobuf.InvalidProtocolBufferException {
        Builder builder = newBuilder();
        try {
          builder.mergeFrom(input, extensionRegistry);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
          throw e.setUnfinishedMessage(builder.buildPartial());
        } catch (com.google.protobuf.UninitializedMessageException e) {
          throw e.asInvalidProtocolBufferException().setUnfinishedMessage(builder.buildPartial());
        } catch (java.io.IOException e) {
          throw new com.google.protobuf.InvalidProtocolBufferException(e)
              .setUnfinishedMessage(builder.buildPartial());
        }
        return builder.buildPartial();
      }
    };

    public static com.google.protobuf.Parser<AndroidSystemPropertyConfig> parser() {
      return PARSER;
    }

    @java.lang.Override
    public com.google.protobuf.Parser<AndroidSystemPropertyConfig> getParserForType() {
      return PARSER;
    }

    @java.lang.Override
    public perfetto.protos.AndroidSystemPropertyConfigOuterClass.AndroidSystemPropertyConfig getDefaultInstanceForType() {
      return DEFAULT_INSTANCE;
    }

  }

  private static final com.google.protobuf.Descriptors.Descriptor
    internal_static_perfetto_protos_AndroidSystemPropertyConfig_descriptor;
  private static final 
    com.google.protobuf.GeneratedMessageV3.FieldAccessorTable
      internal_static_perfetto_protos_AndroidSystemPropertyConfig_fieldAccessorTable;

  public static com.google.protobuf.Descriptors.FileDescriptor
      getDescriptor() {
    return descriptor;
  }
  private static  com.google.protobuf.Descriptors.FileDescriptor
      descriptor;
  static {
    java.lang.String[] descriptorData = {
      "\nCprotos/perfetto/config/android/android" +
      "_system_property_config.proto\022\017perfetto." +
      "protos\"E\n\033AndroidSystemPropertyConfig\022\017\n" +
      "\007poll_ms\030\001 \001(\r\022\025\n\rproperty_name\030\002 \003(\t"
    };
    descriptor = com.google.protobuf.Descriptors.FileDescriptor
      .internalBuildGeneratedFileFrom(descriptorData,
        new com.google.protobuf.Descriptors.FileDescriptor[] {
        });
    internal_static_perfetto_protos_AndroidSystemPropertyConfig_descriptor =
      getDescriptor().getMessageTypes().get(0);
    internal_static_perfetto_protos_AndroidSystemPropertyConfig_fieldAccessorTable = new
      com.google.protobuf.GeneratedMessageV3.FieldAccessorTable(
        internal_static_perfetto_protos_AndroidSystemPropertyConfig_descriptor,
        new java.lang.String[] { "PollMs", "PropertyName", });
  }

  // @@protoc_insertion_point(outer_class_scope)
}