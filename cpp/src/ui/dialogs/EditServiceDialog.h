#pragma once

#include "models/ServiceConfig.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace nyx::ui {

class EditServiceDialog : public QDialog {
    Q_OBJECT

public:
    explicit EditServiceDialog(const nyx::models::ServiceConfig &config, QWidget *parent = nullptr);

    nyx::models::ServiceConfig getServiceConfig() const { return m_updatedConfig; }

private slots:
    void onAccept();

private:
    void initUi();
    void populateFields();
    void updateIconPreview(const QString &iconType);
    void browseIcon(const QString &iconType);

    nyx::models::ServiceConfig m_originalConfig;
    nyx::models::ServiceConfig m_updatedConfig;

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
