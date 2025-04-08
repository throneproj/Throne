#include "include/sys/linux/request.h"
OrgFreedesktopPortalRequestInterface::OrgFreedesktopPortalRequestInterface(
  const QString& service,
  const QString& path,
  const QDBusConnection& connection,
  QObject* parent)
  : QDBusAbstractInterface(service,
                           path,
                           staticInterfaceName(),
                           connection,
                           parent)
{}

OrgFreedesktopPortalRequestInterface::~OrgFreedesktopPortalRequestInterface() =
  default;
