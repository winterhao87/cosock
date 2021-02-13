local epoll = require("epoll")
local sock = require("sock")

local ep, err = epoll.create(1024)
if err then error(err) end

local cli, err = sock.new(">tcp:127.0.0.1:8000")
if err then error(err) end

local cli_fd, err = cli:fd()
if err then error(err) end

local cli_fd_str = "" .. cli_fd
print("cli fd=".. cli_fd_str)

_, err = ep:add(cli_fd, epoll.EPOLLOUT)
if err then error(err) end

local fd_to_sk = {}
fd_to_sk[cli_fd_str] = {sk = cli, cnt = 1, name="C1"}


local cli, err = sock.new(">tcp:127.0.0.1:8000")
if err then error(err) end

local cli_fd, err = cli:fd()
if err then error(err) end

_, err = ep:add(cli_fd, epoll.EPOLLOUT)
if err then error(err) end

local cli_fd_str = "" .. cli_fd
print("cli fd=".. cli_fd_str)
fd_to_sk[cli_fd_str] = {sk = cli, cnt = 1, name="C2"}

function do_cli(r, c, ev)
    local sk = c.sk
    local cnt = c.cnt
    local name = c.name

    if ev == epoll.EPOLLOUT then
        local data = name .. " hello " .. cnt .. "th"
        local nbytes, err = sk:write(data)
        if err then error(err) end
        print("sk:write ", nbytes)

        c.cnt = cnt + 1
        if (cnt > 5) then
            r:del(sk:fd())
            sk:close()
            fd_to_sk["" .. sk:fd()] = nil
            return
        end

        r:modify(sk:fd(), epoll.EPOLLIN)
        return
    end

    if ev == epoll.EPOLLIN then
        local n, data, err = sk:read()
        if err then
            r:del(sk:fd())
            sk:close()
            fd_to_sk["" .. sk:fd()] = nil
            print(err)
            return
        end

        if n < 0 then
            print("handle_cli n < 0")
            return
        end

        if n == 0 then
            print("EOF")
            r:del(sk:fd())
            sk:close()
            fd_to_sk["" .. sk:fd()] = nil
            return
        end

        print("sock_read: ", data)
        r:modify(sk:fd(), epoll.EPOLLOUT)
        return
    end

    print("unknow ev: ", ev)
end

while true do
    local t_ev, t_fd, err = ep:wait(-1)
    if err then error(err) end
    print("ep:wait ", #t_fd)

    for i, ev in ipairs(t_ev) do
        local fd = t_fd[i]
        local c = fd_to_sk[fd]
        if c == nil then error("sk is nil at fd=" .. fd) end

        do_cli(ep, c, ev)
    end

end