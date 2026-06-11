cmake_minimum_required(VERSION 3.16)

project(zz9000_sdk C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(zz9k_host STATIC
  host/src/zz9k_host.c
)

target_include_directories(zz9k_host
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_library(zz9k_fb_common STATIC
  tools/zz9k-fb-common.c
)

target_include_directories(zz9k_fb_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

add_library(zz9k_image_window STATIC
  tools/zz9k-image-window.c
)

target_include_directories(zz9k_image_window
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_link_libraries(zz9k_image_window PUBLIC zz9k_host)

add_library(zz9k_picture_viewer STATIC
  tools/zz9k-picture-viewer.c
)

target_include_directories(zz9k_picture_viewer
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_link_libraries(zz9k_picture_viewer PUBLIC zz9k_host zz9k_image_window)

add_library(zz9k_lha_unix STATIC
  tools/lha-unix/bitio.c
  tools/lha-unix/crcio.c
  tools/lha-unix/dhuf.c
  tools/lha-unix/extract.c
  tools/lha-unix/huf.c
  tools/lha-unix/larc.c
  tools/lha-unix/maketbl.c
  tools/lha-unix/maketree.c
  tools/lha-unix/shuf.c
  tools/lha-unix/slide.c
  tools/lha-unix/zz9k_lha_unix.c
  tools/lha-unix/zz9k_lha_unix_support.c
)

target_include_directories(zz9k_lha_unix
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/lha-unix
)

target_compile_definitions(zz9k_lha_unix PRIVATE HAVE_CONFIG_H=1)

add_library(zz9k_library_core STATIC
  amiga/src/zz9k_library.c
)

target_include_directories(zz9k_library_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include
)

target_link_libraries(zz9k_library_core PUBLIC zz9k_host)

add_executable(abi_layout_test
  tests/abi_layout_test.c
)

target_include_directories(abi_layout_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_test(NAME abi_layout_test COMMAND abi_layout_test)

add_executable(host_mailbox_test
  tests/host_mailbox_test.c
)

target_include_directories(host_mailbox_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(host_mailbox_test PRIVATE zz9k_host)
add_test(NAME host_mailbox_test COMMAND host_mailbox_test)

add_executable(host_mailbox_source_test
  tests/host_mailbox_source_test.c
)
add_test(
  NAME host_mailbox_source_test
  COMMAND host_mailbox_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/host/src/zz9k_host.c
)

add_executable(request_builder_test
  tests/request_builder_test.c
)

target_include_directories(request_builder_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_test(NAME request_builder_test COMMAND request_builder_test)

add_executable(caps_helper_test
  tests/caps_helper_test.c
)

target_include_directories(caps_helper_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME caps_helper_test COMMAND caps_helper_test)

add_executable(text_helper_test
  tests/text_helper_test.c
)

target_include_directories(text_helper_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME text_helper_test COMMAND text_helper_test)

add_executable(image_geometry_test
  tests/image_geometry_test.c
)

target_include_directories(image_geometry_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME image_geometry_test COMMAND image_geometry_test)

add_executable(image_helper_test
  tests/image_helper_test.c
)

target_include_directories(image_helper_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME image_helper_test COMMAND image_helper_test)

add_executable(surface_helper_test
  tests/surface_helper_test.c
)

target_include_directories(surface_helper_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME surface_helper_test COMMAND surface_helper_test)

add_executable(crypto_helper_test
  tests/crypto_helper_test.c
)

target_include_directories(crypto_helper_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME crypto_helper_test COMMAND crypto_helper_test)

add_executable(crypto_soft_test
  tests/crypto_soft_test.c
)

target_include_directories(crypto_soft_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

add_test(NAME crypto_soft_test COMMAND crypto_soft_test)

add_executable(cryptobench_logic_test
  tests/cryptobench_logic_test.c
)

target_include_directories(cryptobench_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_test(NAME cryptobench_logic_test COMMAND cryptobench_logic_test)

add_executable(handshake_model_test
  tests/handshake_model_test.c
)

target_include_directories(handshake_model_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

add_test(NAME handshake_model_test COMMAND handshake_model_test)

add_executable(shared_helper_test
  tests/shared_helper_test.c
)

target_include_directories(shared_helper_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_test(NAME shared_helper_test COMMAND shared_helper_test)

add_executable(shared_helper_source_test
  tests/shared_helper_source_test.c
)

add_test(
  NAME shared_helper_source_test
  COMMAND shared_helper_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/include/zz9k/shared.h
)

add_executable(sdk_umbrella_header_test
  tests/sdk_umbrella_header_test.c
)

target_include_directories(sdk_umbrella_header_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_test(NAME sdk_umbrella_header_test COMMAND sdk_umbrella_header_test)

add_executable(reply_decoder_test
  tests/reply_decoder_test.c
)

target_include_directories(reply_decoder_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_test(NAME reply_decoder_test COMMAND reply_decoder_test)

add_executable(image_stream_abi_test
  tests/image_stream_abi_test.c
)

target_include_directories(image_stream_abi_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME image_stream_abi_test COMMAND image_stream_abi_test)

add_executable(image_stream_request_test
  tests/image_stream_request_test.c
)

target_include_directories(image_stream_request_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_test(NAME image_stream_request_test COMMAND image_stream_request_test)

add_executable(image_stream_reply_test
  tests/image_stream_reply_test.c
)

target_include_directories(image_stream_reply_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_test(NAME image_stream_reply_test COMMAND image_stream_reply_test)

add_executable(module_manifest_test
  tests/module_manifest_test.c
)

target_include_directories(module_manifest_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME module_manifest_test COMMAND module_manifest_test)

add_executable(service_registry_test
  tests/service_registry_test.c
)

target_include_directories(service_registry_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME service_registry_test COMMAND service_registry_test)

add_executable(package_script_test
  tests/package_script_test.c
)

add_test(
  NAME package_script_test
  COMMAND package_script_test
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package-m68k-amigaos.ps1
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/package-m68k-amigaos.sh
)

add_executable(docs_library_source_test
  tests/docs_library_source_test.c
)

add_test(
  NAME docs_library_source_test
  COMMAND docs_library_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/docs/zz9k-library.md
)

add_executable(release_smoke_doc_test
  tests/release_smoke_doc_test.c
)

add_test(
  NAME release_smoke_doc_test
  COMMAND release_smoke_doc_test
          ${CMAKE_CURRENT_SOURCE_DIR}/docs/zz9k-release-smoke.md
)

add_executable(amiga_library_metadata_test
  tests/amiga_library_metadata_test.c
)

target_include_directories(amiga_library_metadata_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include
)

add_test(
  NAME amiga_library_metadata_test
  COMMAND amiga_library_metadata_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/fd/zz9k_lib.fd
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/clib/zz9k_protos.h
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/proto/zz9k.h
)

add_executable(mpega_metadata_test
  tests/mpega_metadata_test.c
)

add_test(
  NAME mpega_metadata_test
  COMMAND mpega_metadata_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/fd/mpega_lib.fd
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/fd/mpega.fd
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/libraries/mpega.h
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/clib/mpega_protos.h
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/proto/mpega.h
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/inline/mpega.h
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/pragmas/mpega_pragmas.h
)

add_executable(mpega_resident_source_test
  tests/mpega_resident_source_test.c
)

add_test(
  NAME mpega_resident_source_test
  COMMAND mpega_resident_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/mpega/mpega_resident.c
)

add_executable(mpega_smoke_tool_source_test
  tests/mpega_smoke_tool_source_test.c
)

add_test(
  NAME mpega_smoke_tool_source_test
  COMMAND mpega_smoke_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-mpega-smoke.c
)

add_executable(library_async_test
  tests/library_async_test.c
)

target_link_libraries(library_async_test PRIVATE zz9k_library_core)
add_test(NAME library_async_test COMMAND library_async_test)

add_executable(library_lvo_test
  tests/library_lvo_test.c
)

target_include_directories(library_lvo_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include
)

add_test(NAME library_lvo_test COMMAND library_lvo_test)

add_executable(library_resident_lock_test
  tests/library_resident_lock_test.c
)

target_include_directories(library_resident_lock_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include
)

add_test(
  NAME library_resident_lock_test
  COMMAND library_resident_lock_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/src/zz9k_library_resident.c
)

add_executable(library_irq_source_test
  tests/library_irq_source_test.c
)

add_test(
  NAME library_irq_source_test
  COMMAND library_irq_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/src/zz9k_library_resident.c
)

add_executable(m68k_build_script_test
  tests/m68k_build_script_test.c
)

add_test(
  NAME m68k_build_script_test
  COMMAND m68k_build_script_test
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-m68k-amigaos.ps1
          ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-m68k-amigaos.sh
)

add_executable(proto_inline_clobber_test
  tests/proto_inline_clobber_test.c
)

add_test(
  NAME proto_inline_clobber_test
  COMMAND proto_inline_clobber_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/proto/zz9k.h
)

add_executable(fbtest_logic_test
  tests/fbtest_logic_test.c
)

target_include_directories(fbtest_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(fbtest_logic_test PRIVATE zz9k_fb_common)
add_test(NAME fbtest_logic_test COMMAND fbtest_logic_test)

add_executable(scaletest_logic_test
  tests/scaletest_logic_test.c
)

target_include_directories(scaletest_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(scaletest_logic_test PRIVATE zz9k_fb_common)
add_test(NAME scaletest_logic_test COMMAND scaletest_logic_test)

add_executable(surfaceops_logic_test
  tests/surfaceops_logic_test.c
)

target_include_directories(surfaceops_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(surfaceops_logic_test PRIVATE zz9k_fb_common)
target_link_libraries(surfaceops_logic_test PRIVATE zz9k_image_window)
add_test(NAME surfaceops_logic_test COMMAND surfaceops_logic_test)

add_executable(image_window_logic_test
  tests/image_window_logic_test.c
)

target_include_directories(image_window_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_link_libraries(image_window_logic_test PRIVATE zz9k_image_window)
add_test(NAME image_window_logic_test COMMAND image_window_logic_test)

add_executable(bench_logic_test
  tests/bench_logic_test.c
)

target_include_directories(bench_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

add_test(NAME bench_logic_test COMMAND bench_logic_test)

add_executable(bench_tool_source_test
  tests/bench_tool_source_test.c
)

add_test(
  NAME bench_tool_source_test
  COMMAND bench_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-bench.c
)

add_executable(smoke_logic_test
  tests/smoke_logic_test.c
)

target_include_directories(smoke_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(smoke_logic_test PRIVATE zz9k_host)
add_test(NAME smoke_logic_test COMMAND smoke_logic_test)

add_executable(smoke_tool_source_test
  tests/smoke_tool_source_test.c
)

add_test(
  NAME smoke_tool_source_test
  COMMAND smoke_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-smoke.c
)

add_executable(capability_require_source_test
  tests/capability_require_source_test.c
)

add_test(
  NAME capability_require_source_test
  COMMAND capability_require_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-smoke.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libsmoke.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-scaletest.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-surfaceops.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-bench.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-surface-info.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libdecode.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-jpeg.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-png.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-hash.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-chacha.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-aead.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-fbtest.c
)

add_executable(hash_tool_source_test
  tests/hash_tool_source_test.c
)

add_test(
  NAME hash_tool_source_test
  COMMAND hash_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-hash.c
)

add_executable(inflate_tool_source_test
  tests/inflate_tool_source_test.c
)

add_test(
  NAME inflate_tool_source_test
  COMMAND inflate_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-inflate.c
)

add_executable(archive_tool_source_test
  tests/archive_tool_source_test.c
)

add_test(
  NAME archive_tool_source_test
  COMMAND archive_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-archive.c
)

add_executable(irq_tool_source_test
  tests/irq_tool_source_test.c
)

add_test(
  NAME irq_tool_source_test
  COMMAND irq_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-irqtest.c
)

add_executable(archive_tool_logic_test
  tests/archive_tool_logic_test.c
)

target_include_directories(archive_tool_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(archive_tool_logic_test PRIVATE zz9k_host zz9k_lha_unix)
add_test(NAME archive_tool_logic_test COMMAND archive_tool_logic_test)

add_executable(libdecode_tool_source_test
  tests/libdecode_tool_source_test.c
)

add_test(
  NAME libdecode_tool_source_test
  COMMAND libdecode_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libdecode.c
)

add_executable(libevent_tool_source_test
  tests/libevent_tool_source_test.c
)

add_executable(public_event_wait_header_test
  tests/public_event_wait_header_test.c
)

add_test(
  NAME public_event_wait_header_test
  COMMAND public_event_wait_header_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/include/zz9k/event_wait.h
)

add_test(
  NAME libevent_tool_source_test
  COMMAND libevent_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libevent.c
)

add_executable(libeventbatch_tool_source_test
  tests/libeventbatch_tool_source_test.c
)

add_test(
  NAME libeventbatch_tool_source_test
  COMMAND libeventbatch_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventbatch.c
)

add_executable(libeventcb_tool_source_test
  tests/libeventcb_tool_source_test.c
)

add_test(
  NAME libeventcb_tool_source_test
  COMMAND libeventcb_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventcb.c
)

add_executable(libeventlife_tool_source_test
  tests/libeventlife_tool_source_test.c
)

add_test(
  NAME libeventlife_tool_source_test
  COMMAND libeventlife_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventlife.c
)

add_executable(libeventports_tool_source_test
  tests/libeventports_tool_source_test.c
)

add_test(
  NAME libeventports_tool_source_test
  COMMAND libeventports_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventports.c
)

add_executable(libeventcancel_tool_source_test
  tests/libeventcancel_tool_source_test.c
)

add_test(
  NAME libeventcancel_tool_source_test
  COMMAND libeventcancel_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventcancel.c
)

add_executable(libtest_tool_source_test
  tests/libtest_tool_source_test.c
)

add_test(
  NAME libtest_tool_source_test
  COMMAND libtest_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libtest.c
)

add_executable(libsmoke_tool_source_test
  tests/libsmoke_tool_source_test.c
)

add_test(
  NAME libsmoke_tool_source_test
  COMMAND libsmoke_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libsmoke.c
)

add_executable(library_async_tools_source_test
  tests/library_async_tools_source_test.c
)

add_test(
  NAME library_async_tools_source_test
  COMMAND library_async_tools_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libasync.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libbatch.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libcancel.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libwait.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libwaitbatch.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libevent.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventbatch.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventcb.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventlife.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventports.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-libeventcancel.c
)

add_executable(typed_decode_example_source_test
  tests/typed_decode_example_source_test.c
)

add_test(
  NAME typed_decode_example_source_test
  COMMAND typed_decode_example_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/examples/amiga-typed-decode/zz9k-typed-decode-demo.c
)

add_executable(jpeg_stream_example_source_test
  tests/jpeg_stream_example_source_test.c
)

add_test(
  NAME jpeg_stream_example_source_test
  COMMAND jpeg_stream_example_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/examples/amiga-jpeg-stream/zz9k-jpeg-stream-demo.c
)

add_executable(crypto_example_source_test
  tests/crypto_example_source_test.c
)

add_test(
  NAME crypto_example_source_test
  COMMAND crypto_example_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/examples/amiga-crypto/zz9k-crypto-demo.c
)

add_executable(library_demo_source_test
  tests/library_demo_source_test.c
)

add_test(
  NAME library_demo_source_test
  COMMAND library_demo_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/examples/amiga-library/zz9k-library-demo.c
)

add_executable(jpeg_tool_source_test
  tests/jpeg_tool_source_test.c
)

add_test(
  NAME jpeg_tool_source_test
  COMMAND jpeg_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-jpeg.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-image-window.c
)

add_executable(png_tool_source_test
  tests/png_tool_source_test.c
)

add_test(
  NAME png_tool_source_test
  COMMAND png_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-png.c
)

add_executable(jpeg_tool_logic_test
  tests/jpeg_tool_logic_test.c
)

target_include_directories(jpeg_tool_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(jpeg_tool_logic_test PRIVATE zz9k_host)
target_link_libraries(jpeg_tool_logic_test PRIVATE zz9k_fb_common)
target_link_libraries(jpeg_tool_logic_test PRIVATE zz9k_image_window)
target_link_libraries(jpeg_tool_logic_test PRIVATE zz9k_picture_viewer)
add_test(NAME jpeg_tool_logic_test COMMAND jpeg_tool_logic_test)

add_executable(png_tool_logic_test
  tests/png_tool_logic_test.c
)

target_include_directories(png_tool_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(png_tool_logic_test PRIVATE zz9k_host)
target_link_libraries(png_tool_logic_test PRIVATE zz9k_fb_common)
target_link_libraries(png_tool_logic_test PRIVATE zz9k_image_window)
target_link_libraries(png_tool_logic_test PRIVATE zz9k_picture_viewer)
add_test(NAME png_tool_logic_test COMMAND png_tool_logic_test)

add_executable(picture_view_logic_test
  tests/picture_view_logic_test.c
)

target_include_directories(picture_view_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_link_libraries(picture_view_logic_test PRIVATE zz9k_picture_viewer)

add_test(NAME picture_view_logic_test
  COMMAND picture_view_logic_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-view.c)

add_executable(picture_datatype_source_test
  tests/picture_datatype_source_test.c
)

add_test(
  NAME picture_datatype_source_test
  COMMAND picture_datatype_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/datatypes/zz9k_picture_datatype.c
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-image-window.h
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-image-window.c
)

add_executable(picture_datatype_doc_test
  tests/picture_datatype_doc_test.c
)

add_test(
  NAME picture_datatype_doc_test
  COMMAND picture_datatype_doc_test
          ${CMAKE_CURRENT_SOURCE_DIR}/docs/zz9k-picture-datatype.md
)

add_executable(datatype_probe_source_test
  tests/datatype_probe_source_test.c
)

add_test(
  NAME datatype_probe_source_test
  COMMAND datatype_probe_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-dtprobe.c
)

add_executable(datatype_descriptor_test
  tests/datatype_descriptor_test.c
)

add_test(
  NAME datatype_descriptor_test
  COMMAND datatype_descriptor_test
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/datatypes/descriptors/ZZ9000-JPEG.b64
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/datatypes/descriptors/ZZ9000-PNG.b64
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/datatypes/descriptors/ZZ9000-JPEG.dtid
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/datatypes/descriptors/ZZ9000-PNG.dtid
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/datatypes/descriptors/ZZ9000-JPEG.info
          ${CMAKE_CURRENT_SOURCE_DIR}/amiga/datatypes/descriptors/ZZ9000-PNG.info
)

add_executable(scaletest_source_test
  tests/scaletest_source_test.c
)

add_test(
  NAME scaletest_source_test
  COMMAND scaletest_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-scaletest.c
)

add_executable(services_tool_source_test
  tests/services_tool_source_test.c
)

add_test(
  NAME services_tool_source_test
  COMMAND services_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-services.c
)

add_executable(info_tool_source_test
  tests/info_tool_source_test.c
)

add_test(
  NAME info_tool_source_test
  COMMAND info_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-info.c
)

add_executable(surface_info_source_test
  tests/surface_info_source_test.c
)

add_test(
  NAME surface_info_source_test
  COMMAND surface_info_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-surface-info.c
)

add_executable(mp3_tool_source_test
  tests/mp3_tool_source_test.c
)

add_test(
  NAME mp3_tool_source_test
  COMMAND mp3_tool_source_test
          ${CMAKE_CURRENT_SOURCE_DIR}/tools/zz9k-mp3.c
)

add_executable(mp3_tool_logic_test
  tests/mp3_tool_logic_test.c
)

target_include_directories(mp3_tool_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/host/include
)

target_link_libraries(mp3_tool_logic_test PRIVATE zz9k_host)
add_test(NAME mp3_tool_logic_test COMMAND mp3_tool_logic_test)

add_executable(zz9k_info
  tools/zz9k-info.c
)

target_link_libraries(zz9k_info PRIVATE zz9k_host)

add_executable(zz9k_smoke
  tools/zz9k-smoke.c
)

target_link_libraries(zz9k_smoke PRIVATE zz9k_host)

add_executable(zz9k_services
  tools/zz9k-services.c
)

target_link_libraries(zz9k_services PRIVATE zz9k_host)

add_executable(zz9k_bench
  tools/zz9k-bench.c
)

target_link_libraries(zz9k_bench PRIVATE zz9k_host)

add_executable(zz9k_cryptobench
  tools/zz9k-cryptobench.c
  tools/zz9k-crypto-soft.c
)

target_include_directories(zz9k_cryptobench
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_link_libraries(zz9k_cryptobench PRIVATE zz9k_host)

add_executable(zz9k_hash
  tools/zz9k-hash.c
)

target_link_libraries(zz9k_hash PRIVATE zz9k_host)

add_executable(zz9k_inflate
  tools/zz9k-inflate.c
)

target_link_libraries(zz9k_inflate PRIVATE zz9k_host)

add_executable(zz9k_archive
  tools/zz9k-archive.c
)

target_link_libraries(zz9k_archive PRIVATE zz9k_host zz9k_lha_unix)

add_executable(zz9k_irqtest
  tools/zz9k-irqtest.c
)

target_link_libraries(zz9k_irqtest PRIVATE zz9k_host)

add_executable(zz9k_chacha
  tools/zz9k-chacha.c
)

target_link_libraries(zz9k_chacha PRIVATE zz9k_host)

add_executable(zz9k_aead
  tools/zz9k-aead.c
)

target_link_libraries(zz9k_aead PRIVATE zz9k_host)

add_executable(zz9k_mp3
  tools/zz9k-mp3.c
)

target_link_libraries(zz9k_mp3 PRIVATE zz9k_host)

add_executable(zz9k_jpeg
  tools/zz9k-jpeg.c
)

target_link_libraries(zz9k_jpeg PRIVATE zz9k_host zz9k_fb_common zz9k_image_window zz9k_picture_viewer)

add_library(zz9k_jpeg_view_backend STATIC
  tools/zz9k-jpeg.c
)

target_compile_definitions(zz9k_jpeg_view_backend PRIVATE ZZ9K_JPEG_NO_MAIN=1)

target_link_libraries(zz9k_jpeg_view_backend PUBLIC zz9k_host zz9k_fb_common zz9k_image_window zz9k_picture_viewer)

add_executable(zz9k_png
  tools/zz9k-png.c
)

target_link_libraries(zz9k_png PRIVATE zz9k_host zz9k_fb_common zz9k_image_window zz9k_picture_viewer)

add_library(zz9k_png_view_backend STATIC
  tools/zz9k-png.c
)

target_compile_definitions(zz9k_png_view_backend PRIVATE ZZ9K_PNG_NO_MAIN=1)

target_link_libraries(zz9k_png_view_backend PUBLIC zz9k_host zz9k_fb_common zz9k_image_window zz9k_picture_viewer)

add_executable(zz9k_view
  tools/zz9k-view.c
)

target_link_libraries(zz9k_view PRIVATE
  zz9k_host
  zz9k_fb_common
  zz9k_image_window
  zz9k_picture_viewer
  zz9k_jpeg_view_backend
  zz9k_png_view_backend
)

add_executable(zz9k_surface_info
  tools/zz9k-surface-info.c
)

target_link_libraries(zz9k_surface_info PRIVATE zz9k_host)

add_executable(zz9k_fbtest
  tools/zz9k-fbtest.c
)

target_link_libraries(zz9k_fbtest PRIVATE zz9k_host zz9k_fb_common)

add_executable(zz9k_scaletest
  tools/zz9k-scaletest.c
)

target_link_libraries(zz9k_scaletest PRIVATE zz9k_host zz9k_fb_common)

add_executable(zz9k_surfaceops
  tools/zz9k-surfaceops.c
)

target_link_libraries(zz9k_surfaceops PRIVATE zz9k_host zz9k_fb_common)
target_link_libraries(zz9k_surfaceops PRIVATE zz9k_image_window)

add_executable(zz9k_probe
  tools/zz9k-probe.c
)

target_include_directories(zz9k_probe
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(zz9k_step
  tools/zz9k-step.c
)

target_include_directories(zz9k_step
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
