
#ifndef __EWL_ENUMS_H
#define __EWL_ENUMS_H

enum _ewl_orientation {
	EWL_ORIENTATION_HORISONTAL,
	EWL_ORIENTATION_VERTICAL
};

typedef enum _ewl_orientation Ewl_Orientation;

enum _ewl_position {
	EWL_POSITION_LEFT,
	EWL_POSITION_RIGHT,
	EWL_POSITION_TOP,
	EWL_POSITION_BOTTOM,
	EWL_POSITION_TOP_LEFT,
	EWL_POSITION_TOP_RIGHT,
	EWL_POSITION_BOTTOM_LEFT,
	EWL_POSITION_BOTTOM_RIGHT,
	EWL_POSITION_LEFT_TOP,
	EWL_POSITION_LEFT_BOTTOM,
	EWL_POSITION_RIGHT_TOP,
	EWL_POSITION_RIGHT_BOTTOM
};

typedef enum _ewl_position Ewl_Position;

enum _ewl_state {
	EWL_STATE_NORMAL,
	EWL_STATE_HILITED,
	EWL_STATE_PRESSED,
	EWL_STATE_SELECTED,
	EWL_STATE_DND
};

typedef enum _ewl_state Ewl_State;

#endif
