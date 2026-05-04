-- level_01_events.lua
-- Trigger positions live in camera_config.json. Register callbacks here.

function OnCreate(self)
    Camera.on_complete("vista_reveal", function()
        Log.info("Vista 1 done")
        Events.fire("vista_complete", "vista_reveal")
    end)

    Camera.on_complete("vista_reveal_2", function()
        Log.info("Vista 2 done")
        Events.fire("vista_complete", "vista_reveal_2")
    end)

    Log.info("level_01: ready")
end
