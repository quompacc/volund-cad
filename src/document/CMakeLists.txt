add_library(voelund_document STATIC
    DocumentSerializer.cpp
    LegacyDocumentImporter.cpp
    LegacyBodyIdentity.cpp
    DocumentSnapshot.cpp
    DocumentId.cpp
    DocumentIdGenerator.cpp
    IdentifiedDocumentSnapshot.cpp
    NativeDocumentMetadata.cpp
    NativePrimitiveCodec.cpp
    DocumentSerializerSupport.cpp
    DocumentSerializerWriter.cpp
    ModelDocument.cpp
    ModelDocumentHistory.cpp
    DocumentTransaction.cpp
    SketchConstraintSerialization.cpp
    SketchGeometry.cpp
)

target_include_directories(
    voelund_document
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
    voelund_document
    PUBLIC
        voelund_project_options
        Voelund::Kernel
    PRIVATE
        Voelund::Platform
        TKLCAF
)

add_library(Voelund::Document ALIAS voelund_document)
