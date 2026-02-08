#pragma once

#include <QString>

namespace StyleHelper {
    // Colors
    inline QString blueBg()      { return "#a7c7ff"; }
    inline QString redAlert()    { return "#ff3b30"; }
    inline QString primaryBlue() { return "#007aff"; }
    inline QString darkText()    { return "#0b1120"; }
    inline QString grayText()    { return "#111827"; }
    inline QString white()       { return "#ffffff"; }
    
    // Common styles
    inline QString buttonStyle() {
        return "QPushButton {"
               "  background:" + primaryBlue() + ";"
               "  color:white;"
               "  border:none;"
               "  border-radius:10px;"
               "  font-size:16px;"
               "  font-weight:600;"
               "  padding:12px 24px;"
               "  min-width:120px;"
               "}"
               "QPushButton:hover {"
               "  background:#0066dd;"
               "}"
               "QPushButton:pressed {"
               "  background:#0055bb;"
               "}";
    }
    
    inline QString inputStyle() {
        return "QLineEdit {"
               "  background:rgba(255,255,255,0.96);"
               "  border:1px solid #e5e7eb;"
               "  border-radius:10px;"
               "  padding:12px 16px;"
               "  font-size:15px;"
               "  color:#111827;"
               "}"
               "QLineEdit:focus {"
               "  border:2px solid " + primaryBlue() + ";"
               "}";
    }
    
    inline QString cardStyle() {
        return "background:rgba(255,255,255,0.96);"
               "border-radius:18px;"
               "padding:24px;";
    }
    
    inline QString titleStyle() {
        return "font-size:28px;"
               "font-weight:700;"
               "color:" + darkText() + ";";
    }
    
    inline QString subtitleStyle() {
        return "font-size:20px;"
               "font-weight:600;"
               "color:" + darkText() + ";";
    }
}
