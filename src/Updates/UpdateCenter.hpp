#pragma once
#include <QDialog>
namespace Acheron::Updates {
class UpdateManager;
class UpdateCenter : public QDialog {
public:
    UpdateCenter(UpdateManager &updates, QWidget *parent);
};
}
