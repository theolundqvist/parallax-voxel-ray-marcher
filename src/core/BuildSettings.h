#pragma once

/*
*	0: Debugging disabled (maximum performance)
*	1: Minimal error reporting. Constantly checks glGetError().
*	2: Creates GL debug context, but only logs medium and severe problems.
*	3: Creates GL debug context and enables all GL debugging features.
*/
#define DEBUG_LEVEL 3

/*
*	Enables (1) or disables (0) assertions using the Assert()-call (found in Log.h)
*	Turn off for maximum performance.
*/
#define ENABLE_ASSERT					1

/*
*	Enables (1) or disables (0) input parameter checks using the Param()-call (found in Log.h)
*	Turn off for maximum performance.
*/
#define ENABLE_PARAM_CHECK				1

/*
*	Enables (1) or disables (0) CPU profiling (found in Profiler.h)
*	Turn off for maximum performance.
*/
#define ENABLE_PROFILING				1

/*
*	Enables (1) or disables (0) GL render state inspection (found in GLStateInspection.h)
*	Turn off for maximum performance.
*/
#define ENABLE_GL_STATE_INSPECTION		1
