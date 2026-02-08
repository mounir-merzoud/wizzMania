#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "../utils/StyleHelper.h"

class MapPage : public QWidget {
    Q_OBJECT
    
public:
    explicit MapPage(QWidget* parent = nullptr) : QWidget(parent) {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->setSpacing(16);
        
        QLabel* title = new QLabel("Map", this);
        title->setStyleSheet(StyleHelper::titleStyle());
        
        QLabel* placeholder = new QLabel(
            "Here I will add the map widget later.",
            this
        );
        placeholder->setWordWrap(true);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(
            StyleHelper::cardStyle() +
            "padding:60px;font-size:16px;color:#111827;"
        );
        
        layout->addWidget(title);
        layout->addSpacing(8);
        layout->addWidget(placeholder, 1);
    }
};
