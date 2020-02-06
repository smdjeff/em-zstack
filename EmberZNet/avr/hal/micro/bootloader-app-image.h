/** @file hal/micro/bootloader-app-image.h
 * 
 * @brief Definition of the format of the application bootloader image file.
 *
 * Unless otherwise noted, the EmberNet stack does not use these functions, 
 * and therefore the HAL is not required to implement them. However, many of
 * the supplied example applications do use them.
 *
 * Some functions in this file return an EmberStatus value. See 
 * error-def.h for definitions of all EmberStatus return values.
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.-->   
 */
#ifndef DOXYGEN_SHOULD_SKIP_THIS

/* The image file generated by the PC application etrim.exe begins with a
 * header one BOOTLOADER_SEGMENT_SIZE (default: 64 bytes) long. The header
 * has the following structure:
 * Byte | Contents
 *  0   | Image length in segments, counting the header, LSB.
 *  1   | Image length MSB.
 *  2   | Validity check byte D   (LSB)
 *  3   | Validity check byte C
 *  4   | Validity check byte B
 *  5   | Validity check byte A   (MSB)
 *  6   | IMAGE_SIGNATURE_A
 *  7   | IMAGE_SIGNATURE_B
 *  8   | Image timestamp byte D  (Unix epoch time LSB)
 *  9   | Image timestamp byte C
 *  10  | Image timestamp byte B
 *  11  | Image timestamp byte A  (Unix epoch time MSB)
 *  12  | User-defined / Padding
 * ...  | User-defined / Padding
 *  63  | User-defined / Padding
 */
#define IMAGE_HEADER_OFFSET_LENGTH    0
#define IMAGE_HEADER_OFFSET_VALIDITY  2
#define IMAGE_HEADER_OFFSET_SIGNATURE 6
#define IMAGE_HEADER_OFFSET_TIMESTAMP 8
#define IMAGE_HEADER_OFFSET_USER      12

#define IMAGE_SIGNATURE_A 0xEB
#define IMAGE_SIGNATURE_B 0xBE

#define imageSignatureValid(signature)                    \
   ((signature)[0] == IMAGE_SIGNATURE_A                   \
 && (signature)[1] == IMAGE_SIGNATURE_B)

#endif // DOXYGEN_SHOULD_SKIP_THIS

