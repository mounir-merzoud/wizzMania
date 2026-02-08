#pragma once

#include <QString>

/**
 * @brief Helper de styles pour l'application Médecins Sans Frontières
 * 
 * Design épuré et professionnel inspiré de WhatsApp :
 * - Fond blanc propre
 * - Accents bleu médical
 * - Interface minimaliste et fonctionnelle
 */
namespace StyleHelper {
    // Colors - Palette MSF
    inline QString primaryBlue()   { return "#0066CC"; }  // Bleu médical MSF
    inline QString darkBlue()      { return "#004C99"; }  // Bleu foncé pour hover
    inline QString lightBlue()     { return "#E6F2FF"; }  // Bleu très clair pour fond
    inline QString white()         { return "#FFFFFF"; }  // Blanc pur
    inline QString lightGray()     { return "#F5F5F5"; }  // Gris très clair pour fond
    inline QString borderGray()    { return "#E0E0E0"; }  // Gris pour bordures
    inline QString textDark()      { return "#1F1F1F"; }  // Texte principal
    inline QString textGray()      { return "#666666"; }  // Texte secondaire
    inline QString greenOnline()   { return "#25D366"; }  // Vert WhatsApp pour "en ligne"
    
    // Common styles - Buttons
    inline QString primaryButton() {
        return "QPushButton {"
               "  background:" + primaryBlue() + ";"
               "  color:white;"
               "  border:none;"
               "  border-radius:8px;"
               "  font-size:15px;"
               "  font-weight:600;"
               "  padding:12px 24px;"
               "}"
               "QPushButton:hover {"
               "  background:" + darkBlue() + ";"
               "}"
               "QPushButton:pressed {"
               "  background:#003366;"
               "}";
    }
    
    inline QString secondaryButton() {
        return "QPushButton {"
               "  background:" + white() + ";"
               "  color:" + primaryBlue() + ";"
               "  border:1px solid " + borderGray() + ";"
               "  border-radius:8px;"
               "  font-size:15px;"
               "  font-weight:500;"
               "  padding:12px 24px;"
               "}"
               "QPushButton:hover {"
               "  background:" + lightGray() + ";"
               "}";
    }
    
    // Input fields
    inline QString inputStyle() {
        return "QLineEdit {"
               "  background:" + white() + ";"
               "  border:1px solid " + borderGray() + ";"
               "  border-radius:8px;"
               "  padding:12px 16px;"
               "  font-size:15px;"
               "  color:" + textDark() + ";"
               "}"
               "QLineEdit:focus {"
               "  border:2px solid " + primaryBlue() + ";"
               "  padding:11px 15px;"
               "}";
    }
    
    // Cards
    inline QString cardStyle() {
        return "background:" + white() + ";"
               "border:1px solid " + borderGray() + ";"
               "border-radius:12px;"
               "padding:24px;";
    }
    
    // Typography
    inline QString titleStyle() {
        return "font-size:24px;"
               "font-weight:600;"
               "color:" + textDark() + ";";
    }
    
    inline QString subtitleStyle() {
        return "font-size:18px;"
               "font-weight:600;"
               "color:" + textDark() + ";";
    }
    
    inline QString bodyTextStyle() {
        return "font-size:15px;"
               "color:" + textDark() + ";";
    }
    
    inline QString secondaryTextStyle() {
        return "font-size:14px;"
               "color:" + textGray() + ";";
    }
}

