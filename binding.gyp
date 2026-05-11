{
  "targets": [
    {
      "target_name": "domkeys",
      "sources": [
        "src/binding.cc",
        "src/keycodes/keycode_converter.cc"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "src",
        "src/keycodes"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "NODE_ADDON_API_ENABLE_MAYBE"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "conditions": [
        ["OS==\"mac\"", {
          "sources": [ "src/hook_mac.mm" ],
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "NO",
            "CLANG_CXX_LIBRARY": "libc++",
            "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
            "MACOSX_DEPLOYMENT_TARGET": "10.13",
            "OTHER_CFLAGS": [ "-ObjC++" ]
          },
          "link_settings": {
            "libraries": [
              "$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework",
              "$(SDKROOT)/System/Library/Frameworks/CoreGraphics.framework",
              "$(SDKROOT)/System/Library/Frameworks/AppKit.framework",
              "$(SDKROOT)/System/Library/Frameworks/Carbon.framework"
            ]
          }
        }],
        ["OS==\"win\"", {
          "sources": [ "src/hook_win.cc" ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "AdditionalOptions": [ "/std:c++17" ]
            }
          },
          "libraries": [ "user32.lib" ]
        }]
      ]
    }
  ]
}
