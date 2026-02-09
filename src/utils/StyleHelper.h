#pragma once

#include <QString>

/**
 * @brief Helper de styles pour MSF Messenger
 * Palette officielle MSF: Rouge #D9001A, Gris foncé #3C3C3B, Noir #000000
 */
namespace StyleHelper {
    // MSF Color Palette
    inline QString primaryRed()     { return "#D9001A"; }  // Rouge MSF
    inline QString darkGray()       { return "#3C3C3B"; }  // Gris foncé
    inline QString black()          { return "#000000"; }  // Noir
    inline QString white()          { return "#FFFFFF"; }  // Blanc
    inline QString lightGray()      { return "#F5F5F5"; }  // Fond clair
    inline QString borderGray()     { return "#E5E5E5"; }  // Bordures
    inline QString textLight()      { return "#8E8E93"; }  // Texte secondaire
    inline QString bubbleReceived() { return "#F0F0F0"; }  // Bulle reçue
    inline QString bubbleSent()     { return "#5B6F82"; }  // Bulle envoyée
    
    // Buttons
    inline QString primaryButton() {
        return "QPushButton {"
               "  background:" + primaryRed() + ";"
               "  color:white;"
               "  border:none;"
               "  border-radius:8px;"
               "  padding:12px 24px;"
               "  font-size:15px;"
               "  font-weight:500;"
               "}"
               "QPushButton:hover {"
               "  background:#B80016;"
               "}"
               "QPushButton:pressed {"
               "  background:#A00014;"
               "}";
    }
    
    inline QString secondaryButton() {
        return "QPushButton {"
               "  background:" + white() + ";"
               "  color:" + darkGray() + ";"
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
               "  border-radius:20px;"
               "  padding:10px 16px;"
               "  font-size:14px;"
               "  color:" + darkGray() + ";"
               "}"
               "QLineEdit:focus {"
               "  border:1px solid " + borderGray() + ";"
               "}";
    }
    
    // Cards
    inline QString cardStyle() {
        return "background:" + white() + ";"
               "border-radius:12px;"
               "padding:20px;";
    }
    
    // Typography
    inline QString titleStyle() {
        return "font-size:24px;"
               "font-weight:600;"
               "color:" + darkGray() + ";";
    }
    
    inline QString subtitleStyle() {
        return "font-size:18px;"
               "font-weight:500;"
               "color:" + darkGray() + ";";
    }
    
    inline QString bodyTextStyle() {
        return "font-size:15px;"
               "color:" + darkGray() + ";";
    }
    
    inline QString secondaryTextStyle() {
        return "font-size:14px;"
               "color:" + textLight() + ";";
    }
}
