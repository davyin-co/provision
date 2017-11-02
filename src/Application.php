<?php

namespace Aegir\Provision;

use Aegir\Provision\Command\SaveCommand;
use Aegir\Provision\Command\ServicesCommand;
use Aegir\Provision\Command\ShellCommand;
use Aegir\Provision\Command\StatusCommand;
use Aegir\Provision\Command\VerifyCommand;
use Aegir\Provision\Console\Config;
use Psr\Log\LoggerInterface;
use Psr\Log\LogLevel;
use Symfony\Component\Console\Command\HelpCommand;
use Symfony\Component\Console\Command\ListCommand;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Input\InputOption;
use Symfony\Component\Console\Input\InputDefinition;
use Symfony\Component\Console\Logger\ConsoleLogger;
use Symfony\Component\Console\Output\OutputInterface;
use Symfony\Component\Console\Application as BaseApplication;

//use Symfony\Component\DependencyInjection\ContainerInterface;
//use Drupal\Console\Annotations\DrupalCommandAnnotationReader;
//use Drupal\Console\Utils\AnnotationValidator;
//use Drupal\Console\Core\Application as BaseApplication;


/**
 * Class Application
 *
 * @package Aegir\Provision
 */
class Application extends BaseApplication
{

    /**
     * @var string
     */
    const NAME = 'Aegir Provision';

    /**
     * @var string
     */
    const VERSION = '4.x';

    /**
     * @var string
     */
    const CONSOLE_CONFIG = '.provision.yml';

    /**
     * @var string
     */
    const DEFAULT_TIMEZONE = 'America/New_York';
    
    /**
     * @var LoggerInterface
     */
    public $logger;
    
    /**
     * Application constructor.
     *
     * @param \Symfony\Component\Console\Input\InputInterface   $input
     * @param \Symfony\Component\Console\Output\OutputInterface $output
     *
     * @throws \Exception
     */
    public function __construct(InputInterface $input, OutputInterface $output)
    {
        $this->logger = new ConsoleLogger($output,
            [LogLevel::INFO => OutputInterface::VERBOSITY_NORMAL]
        );
    
        // If no timezone is set, set Default.
        if (empty(ini_get('date.timezone'))) {
            date_default_timezone_set($this::DEFAULT_TIMEZONE);
        }

        // Load Configs
        try {
            $this->config = new Config();
        }
        catch (\Exception $e) {
            throw new \Exception($e->getMessage());
        }

        parent::__construct($this::NAME, $this::VERSION);
    }

    /**
     * @var Config
     */
    private $config;

    /**
     * Getter for Configuration.
     *
     * @return Config
     *                Configuration object.
     */
    public function getConfig()
    {
        return $this->config;
    }

    /**
     * Setter for Configuration.
     *
     * @param Config $config
     *                       Configuration object.
     */
    public function setConfig(Config $config)
    {
        $this->config = $config;
    }

    /**
     * Initializes all the default commands.
     */
    protected function getDefaultCommands()
    {
        $commands[] = new HelpCommand();
        $commands[] = new ListCommand();
        $commands[] = new SaveCommand();
        $commands[] = new ServicesCommand();
//        $commands[] = new ShellCommand();
        $commands[] = new StatusCommand();
        $commands[] = new VerifyCommand();

        return $commands;
    }

    /**
     * {@inheritdoc}
     *
     * Adds "--target" option.
     */
    protected function getDefaultInputDefinition()
    {
        $inputDefinition = parent::getDefaultInputDefinition();
        $inputDefinition->addOption(
          new InputOption(
            '--target',
            '-t',
            InputOption::VALUE_NONE,
            'The target context to act on.'
          )
        );

        return $inputDefinition;
    }
    
    /**
     * Lookup and return all contexts as found in files.
     *
     * @return array
     */
    function getAllContexts($name = '') {
        $contexts = [];
        $finder = new \Symfony\Component\Finder\Finder();
        $finder->files()->name('*' . $name . '.yml')->in($this->config->get('config_path') . '/provision');
        foreach ($finder as $file) {
            list($context_type, $context_name) = explode('.', $file->getFilename());
            $class = '\Aegir\Provision\Context\\' . ucfirst($context_type) . "Context";
            $contexts[$context_name] = new $class($context_name, $this->config->all(), $this);
            $contexts[$context_name]->logger = $this->logger;
        }

        if ($name && isset($contexts[$name])) {
            return $contexts[$name];
        }
        elseif ($name && !isset($contexts[$name])) {
            return NULL;
        }
        else {
            return $contexts;
        }
    }
    
    /**
     * Load the Aegir context with the specified name.
     *
     * @param $name
     *
     * @return \Aegir\Provision\Context
     * @throws \Exception
     */
    function getContext($name) {
        if (empty($this->getAllContexts($name))) {
            throw new \Exception('Context not found with name: ' . $name);
        }
        return $this->getAllContexts($name);
    }


    /**
     * Load all server contexts.
     *
     * @param null $service
     * @return mixed
     * @throws \Exception
     */
    public function getAllServers($service = NULL) {
        $servers = [];
        $contexts = $this->getAllContexts();
        if (empty($contexts)) {
            throw new \Exception('No contexts found. Use `provision save` to create one.');
        }

        foreach ($contexts as $name => &$context) {
            if ($context->type == 'server') {
                $servers[$name] = $context;
            }
        }
        return $servers;
    }

    /**
     * Get a simple array of all servers, optionally specifying the the service_type to filter by ("http", "db" etc.)
     * @param string $service_type
     * @return array
     */
    public function getServerOptions($service_type = '') {
        $servers = [];
        foreach ($this->getAllServers() as $server) {
            if ($service_type && !empty($server->config['services'][$service_type])) {
                $servers[$server->name] = $server->name . ': ' . $server->config['services'][$service_type]['type'];
            }
            elseif ($service_type == '') {
                $servers[$server->name] = $server->name . ': ' . $server->config['services'][$service_type]['type'];
            }
        }
        return $servers;
    }
}
