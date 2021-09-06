<?php
/**
 * @file
 * Provision Drush commands.
 *
 *
 * This module provides a framework for a Drupal site to manage and install new
 *   Drupal sites, using the command line Drush utility.
 *
 * It allows for pluggable 'provisioning modules' that can extend and modify
 *   the tasks that are taken during installation.
 *
 * Each site has the following commands that can be run on it.
 *
 * Implemented :
 *   install - Install a new Drupal site. The install command uses 3 separate
 *   hooks to do its job, namely hook_pre_provision_install(),
 *   hook_provision_install() and hook_post_provision_install(). verify  -
 *   Recreate all configuration files, to be in synch with changes in the front
 *   end. And test that they are correct. stats   - Return an associated array
 *   of site statistics. (implemented in provision_stats module, is thus
 *   optional) import  - Import the details of an already existing site into
 *   the provisioning framework. This command inspects the settings.php and
 *   generates the site.php file that the framework uses for configuration.
 *   backup  - Generates a tarball containing the sites directory, the site
 *   data configuration and the database dump. This allows the tarball to act as a 'site package', which can be redeployed on other installations, or used for an upgrade. disable - Disable an installed Drupal site. Changes the virtual host config file so that it redirects to provision_disabled_site_redirect_url enable  - Re-enable a site that has already been disabled. Recreates the virtual host file. delete  - In a site context: generates a back up of the site, and then removes all references to it. In a platform context: removes the platform and its vhost config from the server if no sites are currently running on it restore - Revert to a previous backup of the site.
 *
 *   deploy  - Accepts a site package (backup) as argument, and redeploys it,
 *   running the upgrade processes on it. Uses hook_provision_pre_upgrade(),
 *   hook_provision_upgrade() and hook_provision_post_upgrade() hooks, and
 *   allows clean roll back if any errors occur. Will include stringent
 *   checking of module versions, and allow unit tests to be run. lock    -
 *   Lock a platform so that sites cannot be provisioned on it. This does not
 *   disable or delete the platform nor any sites currently provisioned on it.
 *   unlock  - Unlock a platform so that sites can be provisioned on it.
 *
 *   login-reset - Generate a one-time login reset URL.
 */

include_once('../provision/provision.inc');
/**
 * Implementation of hook_drush_command().
 */
function provision_drush_command() {
  $items['cloudrup-install'] = [
    'description' => dt('Install and verify the Hostmaster frontend.'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'arguments' => [
      'example.com' => dt('The URL of the site to install, optional (default: %host).', ['%host' => provision_fqdn()]),
    ],
    'options' => [
      'http_service_type' => dt('Webserver type to configure (default: %webserver)', ['%webserver' => 'apache']),
      'aegir_db_host' => dt('Database host to connect to (default: %host)', ['%host' => 'localhost']),
      'aegir_db_user' => dt('Database user to connect as (default: %user)', ['%user' => 'root']),
      'aegir_db_pass' => dt('Database password to use'),
      'aegir_db_port' => dt('Database port to use (default: %port)', ['%port' => '3306']),
      'aegir_db_grant_all_hosts' => dt('Allow connection from arbitrary web hosts. (Default: FALSE)'),
      'client_email' => dt('Email of the first client to create in the frontend'),
      'client_name' => dt('Name of the first client to create in the frontend (default: %user)', ['%user' => 'admin']),
      'cloudrup_root_path' => dt('The root path of cloudrup'),
      'aegir_host' => dt('Fully qualified domain name of the local server (default: %fqdn)', ['%fqdn' => provision_fqdn()]),
      'script_user' => dt('User to run the backend as (default: %user)', ['%user' => provision_current_user()]),
      'web_group' => dt('Group the webserver is running as (default: %group)', ['%group' => _provision_default_web_group()]),
      'http_port' => dt('Port the webserver is running on (default: %port)', ['%port' => '80']),
      'version' => dt('The version of this released. (default: %version)', ['%version' => provision_version()]),
      'aegir_root' => dt('Install aegir in this home directory (default: %home). Do not change unless you know what you are doing.', ['%home' => drush_server_home()]),
      'root' => dt('Install the frontend in this directory (default: %home/hostmaster-%version).', [
        '%home' => drush_server_home(),
        '%version' => provision_version(),
      ]),
      'backend-only' => dt('Install just the backend, and not the frontend UI.'),
      'working-copy' => dt('Keep VCS files when building the hostmaster platform using Drush make.'),
    ],
  ];

  return $items;
}


/**
 * @file install the hostmaster system
 */

/**
 * Implements drush_COMMAND_pre_validate().
 *
 * Ensure the database cannot be accessed by anonymous users, as it will
 * otherwise fail later in the install, and thus be harder to recover from.
 *
 * @see Provision_Service_db_mysql::grant_host().
 */
function drush_provision_cloudrup_install_pre_validate() {
  $command = sprintf('mysql -u intntnllyInvalid -h %s -P %s -e "SELECT VERSION()"', drush_get_option('aegir_db_host', 'localhost'), drush_get_option('aegir_db_port', '3306'));
  drush_shell_exec($command);

  if (preg_match("/Access denied for user 'intntnllyInvalid'@'([^']*)'/", implode('', drush_shell_exec_output()), $match)) {
    return;
  }
  elseif (preg_match("/ERROR 1449 \(HY000\): The user specified as a definer \('intntnllyInvalid'@'([^']*)'\) does not exist/", implode('', drush_shell_exec_output()), $match)) {
    return;
  }
  elseif (preg_match("/Host '([^']*)' is not allowed to connect to/", implode('', drush_shell_exec_output()), $match)) {
    return;
  }
  else {
    return drush_set_error('PROVISION_DB_CONNECT_FAIL', dt('Dummy connection failed to fail. Either your MySQL permissions are too lax, or the response was not understood. See http://is.gd/Y6i4FO for more information. %msg', ['%msg' => join("\n", drush_shell_exec_output())]));
  }
}

function drush_provision_cloudrup_install_validate($site = NULL) {
  // set defaults for this whole script
  // those are settings that are not prompted to the user but still overridable
  drush_set_default('version', provision_version());
  drush_set_default('profile', 'hostmaster');

  // Get values needed to set other defaults
  $version = drush_get_option('version');
  $aegir_root = drush_set_default('aegir_root', drush_server_home());
  $profile = drush_get_option('profile');

  drush_set_default('root', $aegir_root . '/' . $profile . '-' . $version);
  drush_set_default('r', drush_get_option('root'));
  drush_set_default('script_user', provision_current_user());
  drush_set_default('web_group', _provision_default_web_group());
  drush_set_default('http_service_type', 'apache');
  drush_set_default('http_port', '80');
  drush_set_default('aegir_db_user', 'root');
  drush_set_default('aegir_db_port', '3306');
  drush_set_default('client_name', 'admin');
  $aegir_db_user = drush_get_option('aegir_db_user');

  // Generate "makefile" message only if there is one set.
  if (!drush_get_option('cloudrup_root_path', FALSE)) {
    // Don't assume we know the makefile used to build an existing platform
    $makefile_msg = '';
  }
  else {
    drush_set_default('cloudrup_root_path', dirname(__FILE__) . '/cloudrup');
    $makefile_msg = dt("Cloudrup Path: !path\n", ['!path' => drush_get_option('cloudrup_root_path')]);
  }

  // Generate "profile" message only if there is one set.
  if (!drush_get_option('profile', FALSE)) {
    $profile_msg = '';
  }
  else {
    $profile_msg = dt("Aegir install profile: !profile\n", ['!profile' => $profile]);
  }

  drush_print("Aegir $version automated install script");
  drush_print("==============================================================================");

  if (!$site || !drush_get_option('aegir_host', NULL) || !drush_get_option('aegir_db_pass', NULL) || filter_var(drush_get_option('client_email'), FILTER_VALIDATE_EMAIL)) {
    drush_print("Some settings have not been provided and will now be prompted.
Don't worry: you will get to review those settings after the final install");
  }
  // now we prompt the user for settings if not provided or not sane
  if (!$site) {
    $site = drush_prompt(dt("Aegir frontend URL"), provision_fqdn());
  }
  drush_set_option('site', $site);

  drush_set_default('aegir_host', provision_fqdn());
  drush_set_default('aegir_db_host', 'localhost');

  if (is_null(drush_get_option('aegir_db_pass', NULL))) {
    // XXX: may not be portable everywhere?
    system('stty -echo');
    drush_set_option('aegir_db_pass', drush_prompt(dt('MySQL privileged user ("!root") password', ['!root' => $aegir_db_user])));
    system('stty echo');
    print "\n"; // add a newline since the user's didn't print
  }

  if (drush_get_option('aegir_host') == 'localhost') {
    $default_email = 'webmaster@example.com';
  }
  else {
    $default_email = 'webmaster@' . drush_get_option('aegir_host');
  }
  drush_set_default('client_email', $default_email);
  while (!filter_var(drush_get_option('client_email'), FILTER_VALIDATE_EMAIL) && !drush_get_context('DRUSH_AFFIRMATIVE')) {
    drush_set_option('client_email', drush_prompt(dt("Admin user e-mail"), $default_email));
  }

  drush_print(dt('
This script will operate the following changes in your system:

1. Create server-level configuration directories
2. Create the Hostmaster frontend platform
3. Install the frontend site
4. Setup the dispatcher (a user cron job)

We are making the following assumptions:
 * you have read and are following the install instructions at:
    http://docs.aegirproject.org/en/3.x/install/
 * the FQDN of this machine is valid and resolves
 * you are executing this script as your "aegir" user

The following settings will be used:
 Aegir frontend URL: !site
 Master server FQDN: !fqdn
 Aegir root: !home
 Aegir user: !user
 Web group: !web
 Web server: !web_server
 Web server port: !web_server_port
 Aegir DB host: !db_host
 Aegir DB user: !db_user
 Aegir DB password: !db_pass
 Aegir DB port: !db_port
 Aegir version: !version
 Aegir platform path: !root
 Admin email: !email
 !cloudrup_root_path
 !profile',
    [
      '!site' => $site,
      '!fqdn' => drush_get_option('aegir_host'),
      '!home' => drush_get_option('aegir_root'),
      '!user' => drush_get_option('script_user'),
      '!web' => drush_get_option('web_group'),
      '!web_server' => drush_get_option('http_service_type'),
      '!web_server_port' => drush_get_option('http_port'),
      '!db_host' => drush_get_option('aegir_db_host'),
      '!db_user' => drush_get_option('aegir_db_user'),
      '!db_pass' => is_null(drush_get_option('aegir_db_pass', NULL, 'process')) ? '<previously set>' : '<prompted>',
      '!db_port' => drush_get_option('aegir_db_port'),
      '!version' => drush_get_option('version'),
      '!root' => 'fake root',
      '!cloudrup_root_path' => $makefile_msg,
      '!profile' => $profile_msg,
      '!email' => drush_get_option('client_email'),
    ]));

  if (!drush_confirm(dt('Do you really want to proceed with the install'))) {
    return drush_set_error('PROVISION_CANCEL_INSTALL', dt('Installation aborted'));
  }

  return TRUE;
}

/**
 * Drush command to install hostmaster.
 */
function drush_provision_hostmaster_install($site = NULL) {
  $version = drush_get_option('version');
  $site = drush_get_option('site', provision_fqdn());
  $aegir_root = drush_get_option('aegir_root');
  $platform = drush_get_option(['r', 'root']);

  $aegir_http_host = drush_get_option('aegir_host');
  $aegir_http_port = drush_get_option('http_port');
  $aegir_db_user = drush_get_option('aegir_db_user');
  $aegir_db_pass = drush_get_option('aegir_db_pass');
  $aegir_db_port = drush_get_option('aegir_db_port');
  $aegir_db_host = drush_get_option('aegir_db_host');
  $aegir_db_grant_all_hosts = drush_get_option('aegir_db_grant_all_hosts');

  $server = '@server_master';
  $master_context = [
    'context_type' => 'server',
    // files
    'remote_host' => $aegir_http_host,
    'aegir_root' => $aegir_root,
    'script_user' => drush_get_option('script_user'),
    // apache or nginx or..
    'http_service_type' => drush_get_option('http_service_type'),
    'http_port' => $aegir_http_port,
    'web_group' => drush_get_option('web_group'),
    'master_url' => "http://" . $site,
    'db_port' => $aegir_db_port,
  ];

  $master_db = sprintf("mysql://%s:%s@%s:%s", urlencode($aegir_db_user), urlencode($aegir_db_pass), $aegir_db_host, $aegir_db_port);
  if ($aegir_http_host == $aegir_db_host) {
    $master_context['db_service_type'] = 'mysql';
    $master_context['master_db'] = $master_db;
    $dbserver = $server;
  }
  else {
    $dbserver = '@server_' . $aegir_db_host;
    $dbserver_context = [
      'remote_host' => $aegir_db_host,
      'context_type' => 'server',
      'db_service_type' => 'mysql',
      'master_db' => $master_db,
      'db_grant_all_hosts' => $aegir_db_grant_all_hosts,
      'db_port' => $aegir_db_port,
    ];
    drush_invoke_process('@none', "provision-save", [$dbserver], $dbserver_context);
    provision_backend_invoke($dbserver, 'provision-verify');
  }
  drush_invoke_process('@none', "provision-save", [$server], $master_context);
  provision_backend_invoke($server, 'provision-verify');

  // exit if an error has occured.
  if (drush_get_error()) {
    drush_log('Caught drush error, ending drush_provision_hostmaster_install', 'error');
    drush_log(print_r(drush_get_error_log(), 1), 'error');
    return FALSE;
  }

  if (drush_get_option('backend-only')) {
    drush_log('Skipping hostmaster frontend installation, backend-only option set.', 'notice');
    return;
  }

  drush_log('Preparing the hostmaster frontend installation.', 'notice');

  $platform_name = '@platform_hostmaster';
  drush_invoke_process('@none', "provision-save", [$platform_name], [
    'context_type' => 'platform',
    'server' => $server,
    'web_server' => $server,
    'root' => $platform,
    'cloudrup_root_path' => drush_get_option('cloudrup_root_path'),
  ]);
  // propagate working-copy args downward
  $options = [];
  if (drush_get_option('working-copy')) {
    $options['working-copy'] = 1;

    # Don't rewrite module info files if using working-copy.
    $options['no-gitinfofile'] = 1;
  }
  provision_backend_invoke($platform_name, 'provision-verify', [], $options);

  // exit if an error has occured.
  if (drush_get_error()) {
    drush_log('Caught drush error, ending drush_provision_hostmaster_install - B', 'debug');
    drush_log(print_r(drush_get_error_log(), 1), 'debug');
    return FALSE;
  }

  drush_set_default('profile', 'hostmaster');
  $profile = drush_get_option('profile');

  $site_name = '@hostmaster';
  drush_invoke_process('@none', "provision-save", [$site_name], [
    'context_type' => 'site',
    'platform' => $platform_name,
    'db_server' => $dbserver,
    'uri' => $site,
    'client_name' => drush_get_option('client_name'),
    'client_email' => drush_get_option('client_email'),
    'profile' => $profile,
    'drush_aliases' => 'hm',
    'environment_settings' =>
      [
        'locked' => 0,
        'pull_disabled' => 0,
        'deploy' =>
          [
            'update' => 1,
            'cache' => 1,
            'revert' => 0,
            'dothooks' => 0,
            'test' => 0,
          ],
        'install_method' =>
          [
            'method' => 'profile',
            'profile' => 'devmaster',
            'profile_other' => 'devmaster',
            'clone_source' => '_other',
            'clone_source_drush' => '',
            'import' => '',
          ],
      ],
  ]);

  drush_log('Starting with the hostmaster frontend installation.', 'notice');
  $data = provision_backend_invoke($site_name, 'provision-install', [], ['client_email' => drush_get_option('client_email')]);
  provision_backend_invoke($site_name, 'provision-verify');

  // exit if an error has occured.
  if (drush_get_error()) {
    drush_log('Caught drush error, ending drush_provision_hostmaster_install - C', 'debug');
    drush_log(print_r(drush_get_error_log(), 1), 'debug');
    return FALSE;
  }


  drush_print(dt("Initializing the hosting system"));
  drush_invoke_process('@none', 'cache-clear', ['drush']);
  provision_backend_invoke($site_name, 'hosting-setup');

  drush_print("");
  drush_print("==============================================================================");
  drush_print("");
  drush_print("");
  drush_print(dt("Congratulations, Aegir has now been installed."));
  drush_print("");
  drush_print(dt("You should now log in to the Aegir frontend by opening the following link in your web browser:"));
  drush_print("");
  drush_print($data['context']['login_link']);
  drush_print("");
  drush_print("");
  drush_print("==============================================================================");
  drush_print("");
}

/**
 *  Implements drush_hook_post_hostmaster_install().
 */
function drush_provision_post_cloudrup_install() {
  $backend_only = drush_get_option('backend-only');
  if (empty($backend_only)) {
    drush_invoke_process('@hostmaster', 'cache-clear', ['drush']);
  }
}
