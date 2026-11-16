import QtQuick
import QtTest
import "components" as App

// Runs the real AppCard under a given LANGUAGE (see tests/i18n-qml.sh) and
// checks the label comes from the catalog: Portuguese for pt_BR, the English
// source text otherwise.
Item {
    width: 400
    height: 400

    App.AppCard {
        id: card
        width: 260
        appName: "Vivaldi"
        appDescription: "Browser"
        available: true
        installed: false
        busy: false
        actionType: "flatpak"
    }

    TestCase {
        name: "I18n"
        when: windowShown

        function test_language() {
            verify(!/^(C|POSIX|C\..*)$/.test(messagesLocale),
                   "A non-C locale is required (LC_ALL=pt_BR.UTF-8; install locales-all), got " + messagesLocale)
            const button = findChild(card, "installApplicationButton")
            verify(button)
            const pt = expectedLanguage === "pt_BR"
            compare(button.text, pt ? "Instalar" : "Install")
            compare(button.Accessible.name, pt ? "Vivaldi: Instalar" : "Vivaldi: Install")
            card.installed = true
            compare(button.text, pt ? "Remover" : "Remove")
            card.actionType = "web"
            compare(button.text, pt ? "Abrir" : "Open")
            // Plural forms come from the same catalog through i18np().
            compare(i18np("%1 package", "%1 packages", 3), pt ? "3 pacotes" : "3 packages")
        }
    }
}
