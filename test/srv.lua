local cosock = require("cosock")

function handle_cli(sk)
    while true do
        local data, err = sk:read()
        if err then print(err) break end
        -- print("sk.read " .. data)

        local n, err = sk:write(data)
        if err then print(err) break end
        -- print("sk.write " .. n)
    end

    sk:close()
end

cosock.tcp_listen("*", 8000, handle_cli)

cosock.loop()