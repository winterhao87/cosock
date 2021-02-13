local cosock = require("cosock")

function handle_cli(sk)
    if not sk then error("connect fail") end

    local cnt = 0
    while true do
        if cnt > 10 then break end
        local data = "Hello " .. cnt .. "th"
        cnt = cnt + 1

        local n, err = sk:write(data)
        if err then print(err) break end
        -- print("sk.write " .. n)

        local data, err = sk:read()
        if err then print(err) break end
        print("sk.read " .. data)
    end

    sk:close()
end

cosock.tcp_connect("127.0.0.1", 8000, handle_cli)
cosock.tcp_connect("127.0.0.1", 8000, handle_cli)

cosock.loop()