#include "listener.h"

listener::listener(network* instance, imanager* manager)
{
    network_ = instance;
    manager_ = manager;
    listenfd_ = -1;
}

listener::~listener()
{

}

bool listener::listen(const char* ip, int port)
{
    listenfd_ = net_listen(ip, port);
    if (listenfd_ < 0)
    {
        return false;
    }

    network_->add_event(this, listenfd_, EVENT_READ);
    return true;
}

void listener::on_closed(int number, int error)
{
    manager_->on_closed(number, error);
}

void listener::on_package(int number, char* data, int len)
{
    manager_->on_package(number, data, len);
}

void listener::on_event(int events)
{
    assert(events & EVENT_READ);
    sockaddr_in remote;
    socket_t fd = net_accept(listenfd_, remote);
    if (fd < 0)
    {
        return;
    }

    session* object = new session(network_, this);
    if (!object->init(fd))
    {
        delete object;
        return;
    }

    network_->add_object(object);
    manager_->on_accept(object->get_number(), 0);
}

void listener::send(char* data, int len)
{

}

void listener::close()
{

}