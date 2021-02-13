module(..., package.seeall)

local epoll = require("epoll")
local sock = require("sock")

if not epoll then error("require epoll fail") end
if not sock then error("require sock fail") end
print(sock.version())
print(epoll.version())

local ep, err = epoll.create(1024)
if err then error(err) end

local fd_to_co = {_size = 0}
local add_co = function (fd, co)
    fd_to_co[fd] = co
    fd_to_co._size = fd_to_co._size + 1
end
local del_co = function (fd)
    fd_to_co[fd] = nil 
    fd_to_co._size = fd_to_co._size - 1
end
local active_fd_nums = function()
    return fd_to_co._size
end

local make_sock = function(r, sk)
    local r = r or ep
    local sk = sk
    if not sk then error("sk is nil") end
    local fd = sk:fd()

    local obj = {_sk = sk, _r = r, _fd = fd}

    -- function obj.readn(N)
    --     local buf = {}
    --     local n = 0
    --     while len < N do
    --         local data, err = obj.read()
    --         if err then return table.concat(buf), err end
            
    --         table.insert(buf, data)
    --         n = n + #data
    --     end
    --     return table.concat(buf)
    -- end

    function obj.read(self, flag)
        while true do
            local n, data, err = self._sk:read()
            if err then return data, err end
            if n == 0 then return nil, "EOF" end
            if n > 0 then return data, nil end

            _, err = self._r:modify(self._fd, epoll.EPOLLIN)
            if err then error(err) end
            coroutine.yield()
        end
    end

    function obj.write(self, data)
        local size = #data

        local n, err = self._sk:write(data)
        if err then return n, err end
        local nb = n

        while nb < size do
            if n == 0 then
                _, err = self._r:modify(self._fd, epoll.EPOLLOUT)
                if err then error(err) end
                coroutine.yield()
            end

            n, err = self._sk:write(string.sub(data, nb, -1))
            if err then break end
            nb = nb + n
        end

        return nb, err
    end

    function obj.close(self)
        self._fd = -1
    end

    function obj.is_closed(self)
        return self._fd == -1
    end

    return obj
end

local do_cli = function(r, sk, f)
    local r = r
    local sk = sk
    local fd = sk:fd()

    r:add(fd, epoll.EPOLLERR)
    f(make_sock(r, sk))
    r:del(fd)
    sk:close()
    del_co(tostring(fd))
end

local do_connect = function(r, sk, f)
    local fd = sk:fd()
    r:add(fd, epoll.EPOLLOUT + epoll.EPOLLERR)
    local ev = coroutine.yield()
    if ev == epoll.EPOLLOUT then
        r:modify(fd, epoll.EPOLLERR)
        f(make_sock(r, sk))
    else
        f(nil)
    end

    r:del(fd)
    sk:close()
    del_co(tostring(fd))
end

function tcp_connect(ip, port, f)
    local r = ep
    local addr = ip .. ":" .. port
    local info = ">tcp:" .. addr

    local sk, err = sock.new(info)
    if err then return err end

    local co = coroutine.create(do_connect)
    coroutine.resume(co, r, sk, f)
    add_co(tostring(sk:fd()), co)
    return nil
end

function tcp_listen(ip, port, f)
    local addr = ip .. ":" .. port
    local info = "@tcp:" .. addr 
    local sk, err = sock.new(info)
    if err then error(err) end
    local r = ep

    _, err = r:add(sk:fd(), epoll.EPOLLIN)
    if err then error(err) end
    local co = coroutine.create(function(r, sk, addr, f)
        local r = r
        local sk = sk
        local addr = addr

        while true do
            local new_sk, err = sk:accept()
            if err then error(err) end

            if new_sk then
                local new_fd, err = new_sk:fd()
                if err then error(err) end

                local new_fd_str = "" .. new_fd
                print("srv(" .. addr .. ") accept: " .. new_fd_str)

                local co = coroutine.create(do_cli)
                coroutine.resume(co, r, new_sk, f)
                add_co(new_fd_str, co)
            else
                coroutine.yield()
            end
        end
    end)

    coroutine.resume(co, ep, sk, addr, f)
    add_co(tostring(sk:fd()), co)    
end

function loop()
    local r = ep
    while true do
        if active_fd_nums() == 0 then print("exit loop") return end

        local t_ev, t_fd, err = r:wait(-1)
        if err then error(err) end
        -- print("ep:wait ", #t_fd)

        for i, ev in ipairs(t_ev) do
            local fd = t_fd[i]
            local co = fd_to_co[fd]

            if co then
                coroutine.resume(co, ev)
            else
                print("co is nil at fd=" .. fd)
            end
        end
    end
end

return _M
