local epoll = require("epoll")
local sock = require("sock")

local ep, err = epoll.create(1024)
if err then error(err) end

local srv, err = sock.new("@tcp:*:8000")
if err then error(err) end

local srv_fd, err = srv:fd()
if err then error(err) end

local srv_fd_str = "" .. srv_fd
print("srv fd: ", srv_fd_str)

_, err = ep:add(srv_fd, epoll.EPOLLIN)
if err then error(err) end

local fd_to_sk = {}
fd_to_sk[srv_fd_str] = srv

function do_srv(r, sk, ev)
    local new_sk, err = sk:accept()
    if err then error(err) end

    local new_fd, err = new_sk:fd()
    if err then error(err) end

    local new_fd_str = "" .. new_fd
    print("srv accept: ", new_fd_str)

    _, err = r:add(new_fd, epoll.EPOLLIN)
    if err then error(err) end
    fd_to_sk[new_fd_str] = new_sk
end

function handle_cli(r, sk, ev)
    local n, data, err = sk:read()
    if err then
        r:del(sk:fd())
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
    n, err = sk:write(data)
    if err then error(err) end
    if n ~= #data then error("sk:write fail") end
end

while true do
    local t_ev, t_fd, err = ep:wait(-1)
    if err then error(err) end
    print("ep:wait ", #t_fd)

    for i, ev in ipairs(t_ev) do
        local fd = t_fd[i]
        local sk = fd_to_sk[fd]
        if sk == nil then error("sk is nil at fd=" .. fd) end

        if fd == srv_fd_str then
            do_srv(ep, sk, ev)
        else
            handle_cli(ep, sk, ev)
        end
    end
end
