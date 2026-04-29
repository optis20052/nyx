#pragma once

#include "models/ServiceConfig.h"

#include <QDialog>
#include <QHash>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace nyx::ui {

class AddServiceDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddServiceDialog(QWidget *parent = nullptr);

    nyx::models::ServiceConfig getServiceConfig() const { return m_serviceConfig; }

private slots:
    void onServiceSelected(int index);
    void onAccept();

private:
    void initUi();
    void loadAvailableServices();
    void updateIconPreview(const QString &iconType);
    void browseIcon(const QString &iconType);

    nyx::models::ServiceConfig m_serviceConfig;

    // Map display_name -> (service_name, service_type)
    QHash<QString, QPair<QString, QString>> m_availableServices;

    QComboBox *m_serviceCombo = nullptr;
    QLineEdit *m_nameInput = nullptr;
    QLineEdit *m_displayNameInput = nullptr;
    QLineEdit *m_lightIconInput = nullptr;
    QLineEdit *m_darkIconInput = nullptr;
    QLabel *m_lightIconPreview = nullptr;
    QLabel *m_darkIconPreview = nullptr;
    QLineEdit *m_serviceTypeInput = nullptr;
    QCheckBox *m_autoStartCheckbox = nullptr;
    QCheckBox *m_enabledCheckbox = nullptr;

    QString m_lightIconPath;
    QString m_darkIconPath;
};

} // namespace nyx::ui
