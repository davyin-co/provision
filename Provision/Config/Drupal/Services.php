<?php
/**
 * @file
 * Provides the Provision_Config_Drupal_Settings class.
 */

class Provision_Config_Drupal_Services extends Provision_Config {
  public $template = 'provision_drupal_services_8.tpl.php';
  public $description = 'Drupal services.yml file';
  public $creds = array();
  protected $mode = 0440;

  function filename() {
    return $this->site_path . '/services.yml';
  }

  function process() {
    $this->version = provision_version();
    $this->api_version = provision_api_version();

    $data = [
      'parameters' => [
        'renderer.config' => [
          'required_cache_contexts' => ['languages:language_interface', 'theme', 'user.permissions', 'url.path'],
          'auto_placeholder_conditions' => [
            'max-age' => 0,
            'contexts' => ['session', 'user'],
            'tags' => [],
          ]
        ]
      ]
    ];
    drush_command_invoke_all('provision_drupal_services', d()->uri, $data);
    
    $this->data['content'] = Symfony\Component\Yaml\Yaml::dump($data,4);
    if(empty($data)){
      $this->data['content'] = '';
    }

    $this->group = $this->platform->server->web_group;
  }
}
