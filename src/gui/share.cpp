/*
 * Copyright (C) by Roeland Jago Douma <rullzer@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "share.h"
#include "ocssharejob.h"
#include "account.h"

#include <QUrl>

namespace OCC {

Share::Share(AccountPtr account, 
             const QString& id, 
             const QString& path, 
             const ShareType shareType,
             const Permissions permissions,
             const QSharedPointer<Sharee> shareWith)
: _account(account),
  _id(id),
  _path(path),
  _shareType(shareType),
  _permissions(permissions),
  _shareWith(shareWith)
{

}

AccountPtr Share::account() const
{
    return _account;
}

QString Share::getId() const
{
    return _id;
}

Share::ShareType Share::getShareType() const
{
    return _shareType;
}

QSharedPointer<Sharee> Share::getShareWith() const
{
    return _shareWith;
}

void Share::setPermissions(Permissions permissions)
{
    OcsShareJob *job = new OcsShareJob(_account);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), SLOT(slotPermissionsSet(QVariantMap, QVariant)));
    connect(job, SIGNAL(ocsError(int, QString)), SLOT(slotOcsError(int, QString)));
    job->setPermissions(getId(), permissions);
}

void Share::slotPermissionsSet(const QVariantMap &, const QVariant &value)
{
    _permissions = (Permissions)value.toInt();
    emit permissionsSet();
}

Share::Permissions Share::getPermissions() const
{
    return _permissions;
}

void Share::deleteShare()
{
    OcsShareJob *job = new OcsShareJob(_account);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), SLOT(slotDeleted()));
    connect(job, SIGNAL(ocsError(int, const QString &)), SLOT(slotOcsError(int, const QString &)));
    job->deleteShare(getId());
}

void Share::slotDeleted()
{
    emit shareDeleted();
}

void Share::slotOcsError(int statusCode, const QString &message)
{
    emit serverError(statusCode, message);   
}

QUrl LinkShare::getLink() const
{
    return _url;
}

QDate LinkShare::getExpireDate() const
{
    return _expireDate;
}

bool LinkShare::isPasswordSet() const
{
    return _passwordSet;
}

LinkShare::LinkShare(AccountPtr account,
                     const QString& id,
                     const QString& path,
                     Permissions permissions,
                     bool passwordSet,
                     const QUrl& url,
                     const QDate& expireDate)
: Share(account, id, path, Share::TypeLink, permissions),
  _passwordSet(passwordSet),
  _expireDate(expireDate),
  _url(url)
{

}

bool LinkShare::getPublicUpload()
{
    return ((_permissions & PermissionUpdate) &&
            (_permissions & PermissionCreate));
}

void LinkShare::setPublicUpload(bool publicUpload)
{
    OcsShareJob *job = new OcsShareJob(_account);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), SLOT(slotPublicUploadSet(QVariantMap, QVariant)));
    connect(job, SIGNAL(ocsError(int, QString)), SLOT(slotOcsError(int, QString)));
    job->setPublicUpload(getId(), publicUpload);
}

void LinkShare::slotPublicUploadSet(const QVariantMap&, const QVariant &value)
{
    if (value.toBool()) {
        _permissions = PermissionRead | PermissionUpdate | PermissionCreate;
    } else {
        _permissions = PermissionRead;
    }

    emit publicUploadSet();
}

void LinkShare::setPassword(const QString &password)
{
    OcsShareJob *job = new OcsShareJob(_account);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), SLOT(slotPasswordSet(QVariantMap, QVariant)));
    connect(job, SIGNAL(ocsError(int, QString)), SLOT(slotOcsError(int, QString)));
    job->setPassword(getId(), password);
}

void LinkShare::slotPasswordSet(const QVariantMap&, const QVariant &value)
{
    _passwordSet = value.toString() == "";
    emit passwordSet();
}

void LinkShare::setExpireDate(const QDate &date)
{
    OcsShareJob *job = new OcsShareJob(_account);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), SLOT(slotExpireDateSet(QVariantMap, QVariant)));
    connect(job, SIGNAL(ocsError(int, QString)), SLOT(slotOcsError(int, QString)));
    job->setExpireDate(getId(), date);
}

void LinkShare::slotExpireDateSet(const QVariantMap&, const QVariant &value)
{
    _expireDate = value.toDate();
    emit expireDateSet();
}

ShareManager::ShareManager(AccountPtr account, QObject *parent)
: QObject(parent),
  _account(account)
{

}

void ShareManager::createLinkShare(const QString &path,
                                   const QString &password)
{
    OcsShareJob *job = new OcsShareJob(_account);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), SLOT(slotLinkShareCreated(QVariantMap)));
    connect(job, SIGNAL(ocsError(int, QString)), SLOT(slotOcsError(int, QString)));
    job->createLinkShare(path, password);
}

void ShareManager::slotLinkShareCreated(const QVariantMap &reply)
{
    QString message;
    int code = OcsShareJob::getJsonReturnCode(reply, message);

    /*
     * Before we had decent sharing capabilities on the server a 403 "generally"
     * meant that a share was password protected
     */
    if (code == 403) {
        emit linkShareRequiresPassword();
        return;
    } 

    //Parse share
    auto data = reply.value("ocs").toMap().value("data").toMap();
    QSharedPointer<LinkShare> share(parseLinkShare(data));

    emit linkShareCreated(share);
}

void ShareManager::createShare(const QString& path,
                               const Share::ShareType shareType,
                               const QString shareWith,
                               const Share::Permissions permissions)
{
    OcsShareJob *job = new OcsShareJob(_account);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), SLOT(slotShareCreated(QVariantMap)));
    connect(job, SIGNAL(ocsError(int, QString)), SLOT(slotOcsError(int, QString)));
    job->createShare(path, shareType, shareWith, permissions);
}

void ShareManager::slotShareCreated(const QVariantMap &reply)
{
    //Parse share
    auto data = reply.value("ocs").toMap().value("data").toMap();
    QSharedPointer<Share> share(parseShare(data));

    emit shareCreated(share);
}

void ShareManager::fetchShares(const QString &path)
{
    OcsShareJob *job = new OcsShareJob(_account);
    connect(job, SIGNAL(shareJobFinished(QVariantMap, QVariant)), SLOT(slotSharesFetched(QVariantMap)));
    connect(job, SIGNAL(ocsError(int, QString)), SLOT(slotOcsError(int, QString)));
    job->getShares(path);
}

void ShareManager::slotSharesFetched(const QVariantMap &reply)
{
    auto tmpShares = reply.value("ocs").toMap().value("data").toList();
    const QString versionString = _account->serverVersion();
    qDebug() << Q_FUNC_INFO << versionString << "Fetched" << tmpShares.count() << "shares";

    QList<QSharedPointer<Share>> shares;

    foreach(const auto &share, tmpShares) {
        auto data = share.toMap();

        auto shareType = data.value("share_type").toInt();

        QSharedPointer<Share> newShare;

        if (shareType == Share::TypeLink) {
            newShare = parseLinkShare(data);
        } else {
            newShare = parseShare(data);
        }

        shares.append(QSharedPointer<Share>(newShare));    
    }

    qDebug() << Q_FUNC_INFO << "Sending " << shares.count() << "shares";
    emit sharesFetched(shares);
}

QSharedPointer<LinkShare> ShareManager::parseLinkShare(const QVariantMap &data) {
    QUrl url;

    // From ownCloud server 8.2 the url field is always set for public shares
    if (data.contains("url")) {
        url = QUrl(data.value("url").toString());
    } else if (_account->serverVersionInt() >= (8 << 16)) {
        // From ownCloud server version 8 on, a different share link scheme is used.
        url = QUrl(Account::concatUrlPath(_account->url(), QString("index.php/s/%1").arg(data.value("token").toString())).toString());
    } else {
        QList<QPair<QString, QString>> queryArgs;
        queryArgs.append(qMakePair(QString("service"), QString("files")));
        queryArgs.append(qMakePair(QString("t"), data.value("token").toString()));
        url = QUrl(Account::concatUrlPath(_account->url(), QLatin1String("public.php"), queryArgs).toString());
    }

    QDate expireDate;
    if (data.value("expiration").isValid()) {
       expireDate = QDate::fromString(data.value("expiration").toString(), "yyyy-MM-dd 00:00:00");
    }

    return QSharedPointer<LinkShare>(new LinkShare(_account,
                                                   data.value("id").toString(),
                                                   data.value("path").toString(),
                                                   (Share::Permissions)data.value("permissions").toInt(),
                                                   data.value("share_with").isValid(),
                                                   url,
                                                   expireDate));
}

QSharedPointer<Share> ShareManager::parseShare(const QVariantMap &data)
{
    QSharedPointer<Sharee> sharee(new Sharee(data.value("share_with").toString(),
                                             data.value("share_with_displayname").toString(),
                                             (Sharee::Type)data.value("share_type").toInt()));
    
    return QSharedPointer<Share>(new Share(_account,
                                           data.value("id").toString(),
                                           data.value("path").toString(),
                                           (Share::ShareType)data.value("share_type").toInt(),
                                           (Share::Permissions)data.value("permissions").toInt(),
                                           sharee));
}

void ShareManager::slotOcsError(int statusCode, const QString &message)
{
    emit serverError(statusCode, message);   
}

}
