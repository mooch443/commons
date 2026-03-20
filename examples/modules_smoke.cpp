import commons;
import commons.misc;
import commons.gui;

int main() {
    auto quiet = cmn::bool_setting("quiet");
    auto primitive = cmn::gui::PrimitiveType::Points;
    return quiet ? (primitive == cmn::gui::PrimitiveType::Points ? 0 : 1) : 0;
}
