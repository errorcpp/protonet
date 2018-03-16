#include "session.h"

session::session(network* instance, ibacker* backer)
{
    network_ = instance;
    backer_ = backer;
}

session::~session()
{

}

bool session::init(socket_t fd)
{
    fd_ = fd;
    network_->add_event(this, fd_, EVENT_READ);
    return true;
}

void session::on_event(int events)
{
    if (events & EVENT_READ)
    {
        on_readable();
    }

    if (events & EVENT_WRITE)
    {
        on_writable();
    }
}

void session::on_error(int error)
{
    backer_->on_closed(number_, error);
}

void session::on_readable()
{
    for (;;)
    {
        int recv_len = recv_data(fd_, recvbuf_.data(), recvbuf_.space());
        if (recv_len < 0)
        {
            int error = get_socket_err();
            if (error != 0)
            {
                on_error(error);
            }
            return;
        }
        
        if (recv_len == 0)
        {
            on_error(0);
            return;
        }

        recvbuf_.pop_space(recv_len);
        this->dispatch();
    }
}

void session::on_writable()
{
    while (sendbuf_.size() > 0)
    {
        int send_len = send_data(fd_, sendbuf_.data(), sendbuf_.size());
        if (send_len < 0)
        {
            int error = get_socket_err();
            if (error != 0)
            {
                on_error(error);
            }
            return;
        }

        if (send_len == 0)
        {
            on_error(0);
            return;
        }

        sendbuf_.pop_data(send_len);
    }

    if (sendbuf_.size() <= 0)
    {
        network_->del_event(this, fd_, EVENT_WRITE);
    }
}

void session::send(char* data, int len)
{
    char head[16];
    int head_len = encode_var(head, sizeof(head), len);
    iovec iov[2];
    iov[0].iov_base = head;
    iov[0].iov_len = head_len;
    iov[1].iov_base = data;
    iov[1].iov_len = len;

    if (sendbuf_.size() > 0)
    {
        if (!sendbuf_.push_data(iov, 2, 0))
        {
            on_error(-1);
        }
        return;
    }

    int send_len = send_iovec(fd_, iov, 2);
    if (send_len < 0)
    {
        int error = get_socket_err();
        if (error != 0)
        {
            on_error(error);
            return;
        }

        if (!sendbuf_.push_data(iov, 2, 0))
        {
            on_error(-1);
            return;
        }
        network_->add_event(this, fd_, EVENT_WRITE);
        return;
    }

    if (send_len == 0)
    {
        on_error(0);
        return;
    }

    if (send_len < head_len + len)
    {
        if (!sendbuf_.push_data(iov, 2, send_len))
        {
            on_error(-1);
            return;
        }
        network_->add_event(this, fd_, EVENT_WRITE);
    }
}

void session::close()
{

}

void session::dispatch()
{
    for (;;)
    {
        varint body_len = 0;
        int head_len = decode_var(body_len, recvbuf_.data(), recvbuf_.size());
        if (head_len <= 0)
        {
            break;
        }

        if (recvbuf_.size() < head_len + (int)body_len)
        {
            break;
        }

        recvbuf_.pop_data(head_len);
        backer_->on_package(number_, recvbuf_.data(), (int)body_len);
        recvbuf_.pop_data((int)body_len);
    }

    recvbuf_.trim_data();
}