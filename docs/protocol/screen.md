# Screen

| Code     | Description            |
| -------- | ---------------------- |
| 0x10     | Write to buffer        |
| 0x12     | Fill buffer with color |

## Write to Buffer

### Request Format
```
[byte: 0x10] [uint16: packet_length] [byte: color_format] [variable: pixel_data]
```

`packet_lenght` = width * height * `color_format` + 1

`color_format`: number of bytes per pixel

  - `2` = `RGB565`
  - `3` = `RGB888`

`pixel_data` = width * height * color

Writes pixel data to the buffer. The `length` field indicates the number of bytes.


## Fill Buffer with Color

### Request Format
```
[byte: 0x12] [uint16: packet lenght] [byte: color_format] [variable: color_data]
```

`packet lenght` = `color_format` + 1

`color_format`: number of bytes per pixel

  - `2` = `RGB565`
  - `3` = `RGB888`  

`color_data` = RGB565 

Fills the entire buffer with a single color.

