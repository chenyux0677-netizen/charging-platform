cmake_minimum_required(VERSION 3.5)

project(charging-platform VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Charts(销售统计图表)需要系统安装 libqt6charts6-dev,见 README「环境准备」
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Sql Network Test Charts REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Sql Network Test Charts REQUIRED)

set(PROJECT_SOURCES
    main.cpp

    # 数据核心层
    core/DataSource.h
    core/DataSource.cpp
    core/LocalDataSource.h
    core/LocalDataSource.cpp
    core/RemoteDataSource.h
    core/RemoteDataSource.cpp
    core/TableDef.h
    core/TableDef.cpp
    core/GenericTableModel.h
    core/GenericTableModel.cpp

    # 服务器(数据库中间层)
    server/Server.h
    server/Server.cpp

    # 两端共用
    common/Protocol.h
    common/Protocol.cpp
    common/RoleSelectWindow.h
    common/RoleSelectWindow.cpp
    common/AppContext.h
    common/AppContext.cpp
    common/WidgetUtil.h
    common/WidgetUtil.cpp

    # 用户端
    user/UserLoginWindow.h
    user/UserLoginWindow.cpp
    user/UserMainWindow.h
    user/UserMainWindow.cpp
    user/pages/StationListPage.h
    user/pages/StationListPage.cpp
    user/pages/StationDetailPage.h
    user/pages/StationDetailPage.cpp
    user/pages/ChargingPage.h
    user/pages/ChargingPage.cpp
    user/pages/OrderPage.h
    user/pages/OrderPage.cpp
    user/pages/ProfilePage.h
    user/pages/ProfilePage.cpp
    user/services/MapService.h
    user/services/MapService.cpp
    user/services/ChargeService.h
    user/services/ChargeService.cpp

    # 管理员端
    admin/AdminLoginWindow.h
    admin/AdminLoginWindow.cpp
    admin/AdminMainWindow.h
    admin/AdminMainWindow.cpp
    admin/pages/SalesPage.h
    admin/pages/SalesPage.cpp
    admin/pages/PileStatusPage.h
    admin/pages/PileStatusPage.cpp
    admin/pages/PileManagePage.h
    admin/pages/PileManagePage.cpp
    admin/pages/StationManagePage.h
    admin/pages/StationManagePage.cpp
    admin/pages/UserManagePage.h
    admin/pages/UserManagePage.cpp

    # 资源
    resources/resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(charging-platform
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(charging-platform
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(charging-platform PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Charts
)

# ---- 通信自检工具(独立小程序,不属于主程序) ----
qt_add_executable(net_selftest
    tests/net_selftest.cpp
    core/DataSource.h
    core/DataSource.cpp
    core/TableDef.h
    core/TableDef.cpp
    core/LocalDataSource.h
    core/LocalDataSource.cpp
    core/RemoteDataSource.h
    core/RemoteDataSource.cpp
    common/Protocol.h
    common/Protocol.cpp
    server/Server.h
    server/Server.cpp
)
target_link_libraries(net_selftest PRIVATE
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
)

# ---- 数据库自检工具(独立小程序,不属于主程序) ----
qt_add_executable(db_selftest
    tests/db_selftest.cpp
    core/DataSource.h
    core/DataSource.cpp
    core/TableDef.h
    core/TableDef.cpp
    core/LocalDataSource.h
    core/LocalDataSource.cpp
)
target_link_libraries(db_selftest PRIVATE
    Qt${QT_VERSION_MAJOR}::Sql
)

# ---- 界面自检(脚本化点击登录链路,独立小程序,不属于主程序) ----
qt_add_executable(gui_flow_test
    tests/gui_flow_test.cpp
    core/DataSource.h
    core/DataSource.cpp
    core/TableDef.h
    core/TableDef.cpp
    core/LocalDataSource.h
    core/LocalDataSource.cpp
    core/RemoteDataSource.h
    core/RemoteDataSource.cpp
    common/Protocol.h
    common/Protocol.cpp
    common/AppContext.h
    common/AppContext.cpp
    common/RoleSelectWindow.h
    common/RoleSelectWindow.cpp
    server/Server.h
    server/Server.cpp
    user/UserLoginWindow.h
    user/UserLoginWindow.cpp
    user/UserMainWindow.h
    user/UserMainWindow.cpp
    user/pages/StationListPage.h
    user/pages/StationListPage.cpp
    user/pages/StationDetailPage.h
    user/pages/StationDetailPage.cpp
    user/pages/ChargingPage.h
    user/pages/ChargingPage.cpp
    user/pages/OrderPage.h
    user/pages/OrderPage.cpp
    user/pages/ProfilePage.h
    user/pages/ProfilePage.cpp
    user/services/ChargeService.h
    user/services/ChargeService.cpp
    admin/AdminLoginWindow.h
    admin/AdminLoginWindow.cpp
    admin/AdminMainWindow.h
    admin/AdminMainWindow.cpp
    admin/pages/SalesPage.h
    admin/pages/SalesPage.cpp
    admin/pages/PileStatusPage.h
    admin/pages/PileStatusPage.cpp
    admin/pages/PileManagePage.h
    admin/pages/PileManagePage.cpp
    admin/pages/StationManagePage.h
    admin/pages/StationManagePage.cpp
    admin/pages/UserManagePage.h
    admin/pages/UserManagePage.cpp
)
target_link_libraries(gui_flow_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Test
)

set_target_properties(charging-platform PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(charging-platform)
endif()
