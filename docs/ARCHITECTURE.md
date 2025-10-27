# Project Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        main_new.c                           │
│                    (Application Entry)                      │
└──────────────┬──────────────────────────────────────────────┘
               │
               │ Creates & orchestrates
               │
    ┌──────────┴──────────┬─────────────┬──────────────┐
    │                     │             │              │
    ▼                     ▼             ▼              ▼
┌────────┐          ┌──────────┐   ┌─────────┐   ┌──────────┐
│  LED   │◄─────────│  Camera  │   │  WiFi   │   │WebServer │
│ (GPIO) │ uses     │ (OV2640) │   │(Station)│   │  (HTTP)  │
└────────┘          └──────────┘   └─────────┘   └────┬─────┘
     ▲                   │              │              │
     │                   │              │              │
     │                   │              │              │
     └───────────────────┴──────────────┘              │
              Status notifications                     │
                                                       │
                                                       │ serves
                                                       ▼
                                              ┌────────────────┐
                                              │   Web Browser  │
                                              │  http://IP/    │
                                              └────────────────┘
```

## Component Relationships

```
LED (Base Component)
├── Used by: Camera
├── Used by: WiFi
└── Purpose: Visual status feedback

Camera (Core Component)
├── Uses: LED for status
├── Used by: WebServer
└── Purpose: Image capture

WiFi (Network Component)
├── Uses: LED for connection status
└── Purpose: Network connectivity

WebServer (Interface Component)
├── Uses: Camera for image capture
└── Purpose: HTTP interface to camera
```

## File Organization

```
main/
│
├── Core Application
│   └── main_new.c ─────────► Entry point
│
├── Hardware Abstraction Layer (HAL)
│   ├── led.h/led.c ────────► GPIO LED control
│   └── camera.h/camera.c ──► OV2640 camera driver
│
├── Network Layer
│   ├── wifi_manager.h/c ───► WiFi STA connection
│   └── web_server.h/c ─────► HTTP server
│
└── Configuration
    ├── secrets.h ──────────► WiFi credentials
    ├── secrets.h.template ─► Credentials template
    └── idf_component.yml ──► Dependencies
```

## Data Flow

```
User Request (Browser)
       │
       ▼
   WebServer
       │
       ├─► GET / ──────► HTML Page
       │
       └─► GET /capture
              │
              ▼
           Camera
              │
              ├─► Capture JPEG
              │
              ├─► Return frame buffer
              │
              └─► Blink LED (status)
                     │
                     ▼
                  Browser displays image
```

## Object Lifecycle

```
1. app_main() starts
        │
        ▼
2. Create objects
   led_create()
   camera_create(led)
   wifi_create(ssid, pass, led)
   webserver_create(camera)
        │
        ▼
3. Initialize objects
   led->init()
   camera->init()
   wifi->init()
        │
        ▼
4. Start services
   server->start()
        │
        ▼
5. Run forever (main loop)
        │
        ▼
6. Cleanup (on exit)
   webserver_destroy()
   wifi_destroy()
   camera_destroy()
   led_destroy()
```

## Memory Layout

```
Heap Memory:
├── LED_t (small, ~32 bytes)
├── Camera_t (~512 bytes + config)
├── WiFi_t (~128 bytes)
└── WebServer_t (~64 bytes)

PSRAM (External):
├── Camera frame buffer 1 (~30-50KB)
└── Camera frame buffer 2 (~30-50KB)

Stack:
└── Main task (~8KB)
```
