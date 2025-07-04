// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: protos/perfetto/trace/android/app/statusbarmanager.proto
// Protobuf Java Version: 4.29.3

package perfetto.protos;

public final class Statusbarmanager {
  private Statusbarmanager() {}
  static {
    com.google.protobuf.RuntimeVersion.validateProtobufGencodeVersion(
      com.google.protobuf.RuntimeVersion.RuntimeDomain.PUBLIC,
      /* major= */ 4,
      /* minor= */ 29,
      /* patch= */ 3,
      /* suffix= */ "",
      Statusbarmanager.class.getName());
  }
  public static void registerAllExtensions(
      com.google.protobuf.ExtensionRegistryLite registry) {
  }

  public static void registerAllExtensions(
      com.google.protobuf.ExtensionRegistry registry) {
    registerAllExtensions(
        (com.google.protobuf.ExtensionRegistryLite) registry);
  }
  public interface StatusBarManagerProtoOrBuilder extends
      // @@protoc_insertion_point(interface_extends:perfetto.protos.StatusBarManagerProto)
      com.google.protobuf.MessageOrBuilder {
  }
  /**
   * Protobuf type {@code perfetto.protos.StatusBarManagerProto}
   */
  public static final class StatusBarManagerProto extends
      com.google.protobuf.GeneratedMessage implements
      // @@protoc_insertion_point(message_implements:perfetto.protos.StatusBarManagerProto)
      StatusBarManagerProtoOrBuilder {
  private static final long serialVersionUID = 0L;
    static {
      com.google.protobuf.RuntimeVersion.validateProtobufGencodeVersion(
        com.google.protobuf.RuntimeVersion.RuntimeDomain.PUBLIC,
        /* major= */ 4,
        /* minor= */ 29,
        /* patch= */ 3,
        /* suffix= */ "",
        StatusBarManagerProto.class.getName());
    }
    // Use StatusBarManagerProto.newBuilder() to construct.
    private StatusBarManagerProto(com.google.protobuf.GeneratedMessage.Builder<?> builder) {
      super(builder);
    }
    private StatusBarManagerProto() {
    }

    public static final com.google.protobuf.Descriptors.Descriptor
        getDescriptor() {
      return perfetto.protos.Statusbarmanager.internal_static_perfetto_protos_StatusBarManagerProto_descriptor;
    }

    @java.lang.Override
    protected com.google.protobuf.GeneratedMessage.FieldAccessorTable
        internalGetFieldAccessorTable() {
      return perfetto.protos.Statusbarmanager.internal_static_perfetto_protos_StatusBarManagerProto_fieldAccessorTable
          .ensureFieldAccessorsInitialized(
              perfetto.protos.Statusbarmanager.StatusBarManagerProto.class, perfetto.protos.Statusbarmanager.StatusBarManagerProto.Builder.class);
    }

    /**
     * Protobuf enum {@code perfetto.protos.StatusBarManagerProto.WindowState}
     */
    public enum WindowState
        implements com.google.protobuf.ProtocolMessageEnum {
      /**
       * <code>WINDOW_STATE_SHOWING = 0;</code>
       */
      WINDOW_STATE_SHOWING(0),
      /**
       * <code>WINDOW_STATE_HIDING = 1;</code>
       */
      WINDOW_STATE_HIDING(1),
      /**
       * <code>WINDOW_STATE_HIDDEN = 2;</code>
       */
      WINDOW_STATE_HIDDEN(2),
      ;

      static {
        com.google.protobuf.RuntimeVersion.validateProtobufGencodeVersion(
          com.google.protobuf.RuntimeVersion.RuntimeDomain.PUBLIC,
          /* major= */ 4,
          /* minor= */ 29,
          /* patch= */ 3,
          /* suffix= */ "",
          WindowState.class.getName());
      }
      /**
       * <code>WINDOW_STATE_SHOWING = 0;</code>
       */
      public static final int WINDOW_STATE_SHOWING_VALUE = 0;
      /**
       * <code>WINDOW_STATE_HIDING = 1;</code>
       */
      public static final int WINDOW_STATE_HIDING_VALUE = 1;
      /**
       * <code>WINDOW_STATE_HIDDEN = 2;</code>
       */
      public static final int WINDOW_STATE_HIDDEN_VALUE = 2;


      public final int getNumber() {
        return value;
      }

      /**
       * @param value The numeric wire value of the corresponding enum entry.
       * @return The enum associated with the given numeric wire value.
       * @deprecated Use {@link #forNumber(int)} instead.
       */
      @java.lang.Deprecated
      public static WindowState valueOf(int value) {
        return forNumber(value);
      }

      /**
       * @param value The numeric wire value of the corresponding enum entry.
       * @return The enum associated with the given numeric wire value.
       */
      public static WindowState forNumber(int value) {
        switch (value) {
          case 0: return WINDOW_STATE_SHOWING;
          case 1: return WINDOW_STATE_HIDING;
          case 2: return WINDOW_STATE_HIDDEN;
          default: return null;
        }
      }

      public static com.google.protobuf.Internal.EnumLiteMap<WindowState>
          internalGetValueMap() {
        return internalValueMap;
      }
      private static final com.google.protobuf.Internal.EnumLiteMap<
          WindowState> internalValueMap =
            new com.google.protobuf.Internal.EnumLiteMap<WindowState>() {
              public WindowState findValueByNumber(int number) {
                return WindowState.forNumber(number);
              }
            };

      public final com.google.protobuf.Descriptors.EnumValueDescriptor
          getValueDescriptor() {
        return getDescriptor().getValues().get(ordinal());
      }
      public final com.google.protobuf.Descriptors.EnumDescriptor
          getDescriptorForType() {
        return getDescriptor();
      }
      public static final com.google.protobuf.Descriptors.EnumDescriptor
          getDescriptor() {
        return perfetto.protos.Statusbarmanager.StatusBarManagerProto.getDescriptor().getEnumTypes().get(0);
      }

      private static final WindowState[] VALUES = values();

      public static WindowState valueOf(
          com.google.protobuf.Descriptors.EnumValueDescriptor desc) {
        if (desc.getType() != getDescriptor()) {
          throw new java.lang.IllegalArgumentException(
            "EnumValueDescriptor is not for this type.");
        }
        return VALUES[desc.getIndex()];
      }

      private final int value;

      private WindowState(int value) {
        this.value = value;
      }

      // @@protoc_insertion_point(enum_scope:perfetto.protos.StatusBarManagerProto.WindowState)
    }

    /**
     * Protobuf enum {@code perfetto.protos.StatusBarManagerProto.TransientWindowState}
     */
    public enum TransientWindowState
        implements com.google.protobuf.ProtocolMessageEnum {
      /**
       * <code>TRANSIENT_BAR_NONE = 0;</code>
       */
      TRANSIENT_BAR_NONE(0),
      /**
       * <code>TRANSIENT_BAR_SHOW_REQUESTED = 1;</code>
       */
      TRANSIENT_BAR_SHOW_REQUESTED(1),
      /**
       * <code>TRANSIENT_BAR_SHOWING = 2;</code>
       */
      TRANSIENT_BAR_SHOWING(2),
      /**
       * <code>TRANSIENT_BAR_HIDING = 3;</code>
       */
      TRANSIENT_BAR_HIDING(3),
      ;

      static {
        com.google.protobuf.RuntimeVersion.validateProtobufGencodeVersion(
          com.google.protobuf.RuntimeVersion.RuntimeDomain.PUBLIC,
          /* major= */ 4,
          /* minor= */ 29,
          /* patch= */ 3,
          /* suffix= */ "",
          TransientWindowState.class.getName());
      }
      /**
       * <code>TRANSIENT_BAR_NONE = 0;</code>
       */
      public static final int TRANSIENT_BAR_NONE_VALUE = 0;
      /**
       * <code>TRANSIENT_BAR_SHOW_REQUESTED = 1;</code>
       */
      public static final int TRANSIENT_BAR_SHOW_REQUESTED_VALUE = 1;
      /**
       * <code>TRANSIENT_BAR_SHOWING = 2;</code>
       */
      public static final int TRANSIENT_BAR_SHOWING_VALUE = 2;
      /**
       * <code>TRANSIENT_BAR_HIDING = 3;</code>
       */
      public static final int TRANSIENT_BAR_HIDING_VALUE = 3;


      public final int getNumber() {
        return value;
      }

      /**
       * @param value The numeric wire value of the corresponding enum entry.
       * @return The enum associated with the given numeric wire value.
       * @deprecated Use {@link #forNumber(int)} instead.
       */
      @java.lang.Deprecated
      public static TransientWindowState valueOf(int value) {
        return forNumber(value);
      }

      /**
       * @param value The numeric wire value of the corresponding enum entry.
       * @return The enum associated with the given numeric wire value.
       */
      public static TransientWindowState forNumber(int value) {
        switch (value) {
          case 0: return TRANSIENT_BAR_NONE;
          case 1: return TRANSIENT_BAR_SHOW_REQUESTED;
          case 2: return TRANSIENT_BAR_SHOWING;
          case 3: return TRANSIENT_BAR_HIDING;
          default: return null;
        }
      }

      public static com.google.protobuf.Internal.EnumLiteMap<TransientWindowState>
          internalGetValueMap() {
        return internalValueMap;
      }
      private static final com.google.protobuf.Internal.EnumLiteMap<
          TransientWindowState> internalValueMap =
            new com.google.protobuf.Internal.EnumLiteMap<TransientWindowState>() {
              public TransientWindowState findValueByNumber(int number) {
                return TransientWindowState.forNumber(number);
              }
            };

      public final com.google.protobuf.Descriptors.EnumValueDescriptor
          getValueDescriptor() {
        return getDescriptor().getValues().get(ordinal());
      }
      public final com.google.protobuf.Descriptors.EnumDescriptor
          getDescriptorForType() {
        return getDescriptor();
      }
      public static final com.google.protobuf.Descriptors.EnumDescriptor
          getDescriptor() {
        return perfetto.protos.Statusbarmanager.StatusBarManagerProto.getDescriptor().getEnumTypes().get(1);
      }

      private static final TransientWindowState[] VALUES = values();

      public static TransientWindowState valueOf(
          com.google.protobuf.Descriptors.EnumValueDescriptor desc) {
        if (desc.getType() != getDescriptor()) {
          throw new java.lang.IllegalArgumentException(
            "EnumValueDescriptor is not for this type.");
        }
        return VALUES[desc.getIndex()];
      }

      private final int value;

      private TransientWindowState(int value) {
        this.value = value;
      }

      // @@protoc_insertion_point(enum_scope:perfetto.protos.StatusBarManagerProto.TransientWindowState)
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
      getUnknownFields().writeTo(output);
    }

    @java.lang.Override
    public int getSerializedSize() {
      int size = memoizedSize;
      if (size != -1) return size;

      size = 0;
      size += getUnknownFields().getSerializedSize();
      memoizedSize = size;
      return size;
    }

    @java.lang.Override
    public boolean equals(final java.lang.Object obj) {
      if (obj == this) {
       return true;
      }
      if (!(obj instanceof perfetto.protos.Statusbarmanager.StatusBarManagerProto)) {
        return super.equals(obj);
      }
      perfetto.protos.Statusbarmanager.StatusBarManagerProto other = (perfetto.protos.Statusbarmanager.StatusBarManagerProto) obj;

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
      hash = (29 * hash) + getUnknownFields().hashCode();
      memoizedHashCode = hash;
      return hash;
    }

    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(
        java.nio.ByteBuffer data)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(
        java.nio.ByteBuffer data,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data, extensionRegistry);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(
        com.google.protobuf.ByteString data)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(
        com.google.protobuf.ByteString data,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data, extensionRegistry);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(byte[] data)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(
        byte[] data,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws com.google.protobuf.InvalidProtocolBufferException {
      return PARSER.parseFrom(data, extensionRegistry);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(java.io.InputStream input)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessage
          .parseWithIOException(PARSER, input);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(
        java.io.InputStream input,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessage
          .parseWithIOException(PARSER, input, extensionRegistry);
    }

    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseDelimitedFrom(java.io.InputStream input)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessage
          .parseDelimitedWithIOException(PARSER, input);
    }

    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseDelimitedFrom(
        java.io.InputStream input,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessage
          .parseDelimitedWithIOException(PARSER, input, extensionRegistry);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(
        com.google.protobuf.CodedInputStream input)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessage
          .parseWithIOException(PARSER, input);
    }
    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto parseFrom(
        com.google.protobuf.CodedInputStream input,
        com.google.protobuf.ExtensionRegistryLite extensionRegistry)
        throws java.io.IOException {
      return com.google.protobuf.GeneratedMessage
          .parseWithIOException(PARSER, input, extensionRegistry);
    }

    @java.lang.Override
    public Builder newBuilderForType() { return newBuilder(); }
    public static Builder newBuilder() {
      return DEFAULT_INSTANCE.toBuilder();
    }
    public static Builder newBuilder(perfetto.protos.Statusbarmanager.StatusBarManagerProto prototype) {
      return DEFAULT_INSTANCE.toBuilder().mergeFrom(prototype);
    }
    @java.lang.Override
    public Builder toBuilder() {
      return this == DEFAULT_INSTANCE
          ? new Builder() : new Builder().mergeFrom(this);
    }

    @java.lang.Override
    protected Builder newBuilderForType(
        com.google.protobuf.GeneratedMessage.BuilderParent parent) {
      Builder builder = new Builder(parent);
      return builder;
    }
    /**
     * Protobuf type {@code perfetto.protos.StatusBarManagerProto}
     */
    public static final class Builder extends
        com.google.protobuf.GeneratedMessage.Builder<Builder> implements
        // @@protoc_insertion_point(builder_implements:perfetto.protos.StatusBarManagerProto)
        perfetto.protos.Statusbarmanager.StatusBarManagerProtoOrBuilder {
      public static final com.google.protobuf.Descriptors.Descriptor
          getDescriptor() {
        return perfetto.protos.Statusbarmanager.internal_static_perfetto_protos_StatusBarManagerProto_descriptor;
      }

      @java.lang.Override
      protected com.google.protobuf.GeneratedMessage.FieldAccessorTable
          internalGetFieldAccessorTable() {
        return perfetto.protos.Statusbarmanager.internal_static_perfetto_protos_StatusBarManagerProto_fieldAccessorTable
            .ensureFieldAccessorsInitialized(
                perfetto.protos.Statusbarmanager.StatusBarManagerProto.class, perfetto.protos.Statusbarmanager.StatusBarManagerProto.Builder.class);
      }

      // Construct using perfetto.protos.Statusbarmanager.StatusBarManagerProto.newBuilder()
      private Builder() {

      }

      private Builder(
          com.google.protobuf.GeneratedMessage.BuilderParent parent) {
        super(parent);

      }
      @java.lang.Override
      public Builder clear() {
        super.clear();
        return this;
      }

      @java.lang.Override
      public com.google.protobuf.Descriptors.Descriptor
          getDescriptorForType() {
        return perfetto.protos.Statusbarmanager.internal_static_perfetto_protos_StatusBarManagerProto_descriptor;
      }

      @java.lang.Override
      public perfetto.protos.Statusbarmanager.StatusBarManagerProto getDefaultInstanceForType() {
        return perfetto.protos.Statusbarmanager.StatusBarManagerProto.getDefaultInstance();
      }

      @java.lang.Override
      public perfetto.protos.Statusbarmanager.StatusBarManagerProto build() {
        perfetto.protos.Statusbarmanager.StatusBarManagerProto result = buildPartial();
        if (!result.isInitialized()) {
          throw newUninitializedMessageException(result);
        }
        return result;
      }

      @java.lang.Override
      public perfetto.protos.Statusbarmanager.StatusBarManagerProto buildPartial() {
        perfetto.protos.Statusbarmanager.StatusBarManagerProto result = new perfetto.protos.Statusbarmanager.StatusBarManagerProto(this);
        onBuilt();
        return result;
      }

      @java.lang.Override
      public Builder mergeFrom(com.google.protobuf.Message other) {
        if (other instanceof perfetto.protos.Statusbarmanager.StatusBarManagerProto) {
          return mergeFrom((perfetto.protos.Statusbarmanager.StatusBarManagerProto)other);
        } else {
          super.mergeFrom(other);
          return this;
        }
      }

      public Builder mergeFrom(perfetto.protos.Statusbarmanager.StatusBarManagerProto other) {
        if (other == perfetto.protos.Statusbarmanager.StatusBarManagerProto.getDefaultInstance()) return this;
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

      // @@protoc_insertion_point(builder_scope:perfetto.protos.StatusBarManagerProto)
    }

    // @@protoc_insertion_point(class_scope:perfetto.protos.StatusBarManagerProto)
    private static final perfetto.protos.Statusbarmanager.StatusBarManagerProto DEFAULT_INSTANCE;
    static {
      DEFAULT_INSTANCE = new perfetto.protos.Statusbarmanager.StatusBarManagerProto();
    }

    public static perfetto.protos.Statusbarmanager.StatusBarManagerProto getDefaultInstance() {
      return DEFAULT_INSTANCE;
    }

    private static final com.google.protobuf.Parser<StatusBarManagerProto>
        PARSER = new com.google.protobuf.AbstractParser<StatusBarManagerProto>() {
      @java.lang.Override
      public StatusBarManagerProto parsePartialFrom(
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

    public static com.google.protobuf.Parser<StatusBarManagerProto> parser() {
      return PARSER;
    }

    @java.lang.Override
    public com.google.protobuf.Parser<StatusBarManagerProto> getParserForType() {
      return PARSER;
    }

    @java.lang.Override
    public perfetto.protos.Statusbarmanager.StatusBarManagerProto getDefaultInstanceForType() {
      return DEFAULT_INSTANCE;
    }

  }

  private static final com.google.protobuf.Descriptors.Descriptor
    internal_static_perfetto_protos_StatusBarManagerProto_descriptor;
  private static final 
    com.google.protobuf.GeneratedMessage.FieldAccessorTable
      internal_static_perfetto_protos_StatusBarManagerProto_fieldAccessorTable;

  public static com.google.protobuf.Descriptors.FileDescriptor
      getDescriptor() {
    return descriptor;
  }
  private static  com.google.protobuf.Descriptors.FileDescriptor
      descriptor;
  static {
    java.lang.String[] descriptorData = {
      "\n8protos/perfetto/trace/android/app/stat" +
      "usbarmanager.proto\022\017perfetto.protos\"\372\001\n\025" +
      "StatusBarManagerProto\"Y\n\013WindowState\022\030\n\024" +
      "WINDOW_STATE_SHOWING\020\000\022\027\n\023WINDOW_STATE_H" +
      "IDING\020\001\022\027\n\023WINDOW_STATE_HIDDEN\020\002\"\205\001\n\024Tra" +
      "nsientWindowState\022\026\n\022TRANSIENT_BAR_NONE\020" +
      "\000\022 \n\034TRANSIENT_BAR_SHOW_REQUESTED\020\001\022\031\n\025T" +
      "RANSIENT_BAR_SHOWING\020\002\022\030\n\024TRANSIENT_BAR_" +
      "HIDING\020\003"
    };
    descriptor = com.google.protobuf.Descriptors.FileDescriptor
      .internalBuildGeneratedFileFrom(descriptorData,
        new com.google.protobuf.Descriptors.FileDescriptor[] {
        });
    internal_static_perfetto_protos_StatusBarManagerProto_descriptor =
      getDescriptor().getMessageTypes().get(0);
    internal_static_perfetto_protos_StatusBarManagerProto_fieldAccessorTable = new
      com.google.protobuf.GeneratedMessage.FieldAccessorTable(
        internal_static_perfetto_protos_StatusBarManagerProto_descriptor,
        new java.lang.String[] { });
    descriptor.resolveAllFeaturesImmutable();
  }

  // @@protoc_insertion_point(outer_class_scope)
}
