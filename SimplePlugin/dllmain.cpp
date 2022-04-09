#include "../plugin_sdk/plugin_sdk.hpp"
#include "fake_cursor.h"

PLUGIN_NAME( "GamerFakeCursor" );

PLUGIN_API bool on_sdk_load( plugin_sdk_core* plugin_sdk_good )
{
    DECLARE_GLOBALS( plugin_sdk_good );

    fake_cursor::load();
    return true;
}

PLUGIN_API void on_sdk_unload( )
{
    fake_cursor::unload();
}