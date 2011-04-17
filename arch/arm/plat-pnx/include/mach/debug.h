/*
 * Debug utilities
*/
#ifndef  DEBUG_H
#define  DEBUG_H

#include <linux/kernel.h>


/* debug macros
   debug is dynamically filtered using a dynamic variable.
   Filtering occurs on type of trace (function in/out, trace,
   debug) & also on layer to trace, if several layers to debug
   are there (allows to have hierarchic debug trace with indentation).

   User must provide:
   - CONFIG_DEBUG define: a static switch which enable/disable
                          macros expansions, (typically in makefile)
   - DEBUG_VAR define   : a global variable to dynamically filter type/layers
                          (typically a module parameter)
   - DEBUG_LAYER define : layer to debug (typically 1 by .c)

   Type of debug is defined on 4 bits:
   none  (0x0) => nothing, except critical, warning which are always output
   bit 1 (0x1) => usefull light traces (light on output)
   bit 2 (0x2) => prolog & epilog (functions in/out)
   bit 3 (0x4) => deep debug (heavy on output)
   Types can be mixed together, for example:
   0xF => all traces
   0x3 => prolog/epilog + traces

   In order to identify the layer to debug, just shift the type of debug
   by 4 bits:
   0xT   => Layer 0 with T type
   0x0T  => Layer 1 with T type
   0x00T => Layer 2 with T type

   Different type of debug can be mixed with different layers:
   0x261 => Layer 0 with traces,
                  1 with deep debug+prolog/epilog,
                  2 with prolog/epilog

   Example with prolog/epilog on 4 layers (0x2222):
[  260.953696]  > h264_dec_deinit
[  260.953696]   > H264ViAPIDecFreeLibrary
[  260.962955]    > MO4XDRV_close
[  260.972214]     > vdc_hw_exit
[  260.972214]     < vdc_hw_exit
[  260.981474]    < MO4XDRV_close
[  260.981474]   < H264ViAPIDecFreeLibrary
[  260.990733]  < h264_dec_deinit

   For real example use, check module VDC.
*/

#define INDENT_0 ""
#define INDENT_1 " "
#define INDENT_2 "  "
#define INDENT_3 "   "
#define INDENT_4 "    "
#define INDENT_5 "     "
#define INDENT_6 "      "
#define INDENT_(i) INDENT_##i
#define INDENT(i)  INDENT_(i)


#if defined(CONFIG_DEBUG)
#define PRINT(fmt, args...)			        \
    printk ( fmt "\n", ## args)
#else
	#define PRINT(fmt, args...) /* nothing, its a placeholder */
#endif /* defined(CONFIG_DEBUG) */

#define PTRACE(fmt, args...)                        \
    do                                              \
        if (DEBUG_VAR & (0x1<<(DEBUG_LAYER*4)))     \
            PRINT (KERN_DEBUG "   " fmt, ## args);	\
    while (0)

#define PDEBUG(fmt, args...)                        \
    do                                              \
        if (DEBUG_VAR & (0x4<<(DEBUG_LAYER*4)))     \
            PRINT (KERN_DEBUG "   " fmt, ## args);  \
    while (0)

#define PROLOG(fmt, args...)                                            \
    do                                                                  \
        if (DEBUG_VAR & (0x2<<(DEBUG_LAYER*4)))                         \
            PRINT (KERN_DEBUG ""INDENT(DEBUG_LAYER)"> %s " fmt, __func__, ## args); \
    while (0)

#define EPILOG(fmt, args...)                                            \
    do                                                                  \
        if (DEBUG_VAR & (0x2<<(DEBUG_LAYER*4)))                         \
            PRINT (KERN_DEBUG ""INDENT(DEBUG_LAYER)"< %s " fmt, __func__, ## args); \
    while (0)


#define CRITICAL(fmt, args...)                                          \
	printk(KERN_ERR ""INDENT(DEBUG_LAYER)"! %s " fmt "\n", __func__, ## args)

#define WARNING(fmt, args...)                                           \
	printk(KERN_WARNING ""INDENT(DEBUG_LAYER)"? %s " fmt "\n", __func__, ## args)

#define ASSERT(predicate)                                   \
    do                                                      \
        if (unlikely(!predicate))                           \
            CRITICAL ("Assertion failed: %s", #predicate);	\
            return -EINVAL                                  \
    while (0)


#define LIB_PROLOG(fmt, args...)                                        \
    do                                                                  \
        if (DEBUG_VAR & (0x2<<(DEBUG_LAYER*4)))                         \
            PRINT (KERN_DEBUG ""INDENT(DEBUG_LAYER)" > " fmt, ## args); \
    while (0)


#define LIB_EPILOG(fmt, args...)                                        \
    do                                                                  \
        if (DEBUG_VAR & (0x2<<(DEBUG_LAYER*4)))                         \
            PRINT (KERN_DEBUG ""INDENT(DEBUG_LAYER)" < " fmt, ## args); \
    while (0)

#define LIB_CBPROLOG(fmt, args...)                                      \
    do                                                                  \
        if (DEBUG_VAR & (0x2<<(DEBUG_LAYER*4)))                         \
            PRINT (KERN_DEBUG ""INDENT(DEBUG_LAYER)" > %s " fmt, __func__, ## args); \
    while (0)

#define LIB_CBEPILOG(fmt, args...)                                      \
    do                                                                  \
        if (DEBUG_VAR & (0x2<<(DEBUG_LAYER*4)))                         \
            PRINT (KERN_DEBUG ""INDENT(DEBUG_LAYER)" < %s " fmt, __func__, ## args); \
    while (0)


#endif   /* DEBUG_H */

